#pragma once

#include <pl/api.hpp>

#include <wolv/utils/guards.hpp>

#include <vector>
#include <span>
#include <memory>
#include <map>

namespace pl::hlp {
    inline static api::Section::ChunkAttributes& restIsUnmapped(api::Section::ChunkAttributes& attribs) {
        attribs.type = api::Section::ChunkAttributes::Type::Unmapped;
        attribs.baseAddress += attribs.size;
        attribs.size = std::numeric_limits<size_t>::max() - attribs.baseAddress;
        attribs.writable = false;
        
        return attribs;
    }
}

namespace pl::core {
    class Evaluator;

    class EmptySection : public api::Section {
        size_t size() const override {
            return 0;
        }
        
        IOError resize(size_t) override {
            return "EmptySections cannot be resized";
        }

        IOError readRaw(u64, size_t, ChunkReader&) const override {
            return "EmptySections cannot be read";
        }

        IOError writeRaw(u64, size_t, ChunkWriter&) override {
            return "EmptySections cannot be written";
        }
        
        bool readChunkAttributes(u64, size_t, ChunkAttributesReader& reader) const override {
            ChunkAttributes attribs {
                .type = ChunkAttributes::Type::Unmapped,
                .baseAddress = 0,
                .size = std::numeric_limits<size_t>::max()
            };
            return reader(attribs);
        }
    };

    class ZerosSection : public api::Section {
    public:
        explicit ZerosSection(size_t initialSize)
        : m_size(initialSize)
        {}
        
    protected:
        size_t size() const override {
            return m_size;
        }
        
        IOError resize(size_t newSize) override {
            m_size = newSize;
            return std::nullopt;
        }
        
        bool readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const override;
        
        IOError readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const override;

        IOError writeRaw(u64, size_t, ChunkWriter&) override {
            return "ZerosSections cannot be written";
        }
        
    private:
        size_t m_size;
    };

    template<typename Buffer = std::vector<u8>>
    class InMemorySection : public api::Section {
    public:
        static auto AllocVector(size_t maxSize, size_t initialSize = 0) {
            return std::unique_ptr<InMemorySection<std::vector<u8>>>(new InMemorySection<std::vector<u8>>(maxSize, initialSize));
        }
        static auto WrapVector(size_t maxSize, std::vector<u8>& ref) {
            return std::unique_ptr<InMemorySection<std::vector<u8>&>>(new InMemorySection<std::vector<u8>&>(maxSize, ref));
        }
        
        template<typename... Args>
        InMemorySection(size_t maxSize, Args&&... args) 
        : m_maxSize(maxSize)
        , m_buffer(std::forward<Args>(args)...) 
        {}
        
    protected:
        size_t size() const override {
            return m_buffer.size();
        }
        
        IOError resize(size_t newSize) override {
            if (m_maxSize < newSize) {
                return fmt::format("Expansion beyond maximum size of {} is not permitted. Would overflow my {} bytes", m_maxSize, newSize - m_maxSize);
            }
            
            m_buffer.resize(newSize);
            return std::nullopt;
        }

        bool readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const override {
            ChunkAttributes attribs {
                .type = ChunkAttributes::Type::Generic,
                .baseAddress = 0,
                .size = m_buffer.size(),
                .writable = true
            };
            if (bool stop = reader(attribs)) {
                return true;
            }
            
            if (fromAddress + size < attribs.size) {
                return false;
            }
        
            return reader(hlp::restIsUnmapped(attribs));
        }
        
        IOError readRaw(u64 address, size_t size, ChunkReader& reader) const override {
            return reader(std::span(m_buffer).subspan(address, size));
        }
        
        IOError writeRaw(u64 address, size_t size, ChunkWriter& writer) override {
            return writer(std::span(m_buffer).subspan(address, size));
        }

        size_t m_maxSize;
        Buffer m_buffer;
    };

    class DataSourceSection : public api::Section {
    public:
        using ReaderFunction = std::function<void(u64 fromAddress, u8* into, size_t size)>;
        using WriterFunction = std::function<void(u64 toAddress, const u8* from, size_t size)>;
        
        DataSourceSection(size_t readBufferSize, size_t writeBufferSize);

        void setDataSize(u64 size) {
            m_dataSize = size;
        }
        void setReader(ReaderFunction reader) {
            m_reader = std::move(reader);
        }
        void setWriter(WriterFunction writer) {
            m_writer = std::move(writer);
        }
        
    protected:
        size_t size() const override {
            return m_dataSize;
        }
        
        IOError resize(size_t) override {
            return "ProviderSection cannot be resized";
        }

        IOError readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const override;

        IOError writeRaw(u64 toAddress, size_t size, ChunkWriter& writer) override;

        bool readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const override;
        
    private:
        u64 m_dataSize = 0x00;

        ReaderFunction m_reader;
        WriterFunction m_writer;
                
        mutable bool m_readBufferInUse = false;
        mutable bool m_writeBufferInUse = false;
        
        mutable std::vector<u8> m_readBuffer;
        mutable std::vector<u8> m_writeBuffer;
    };
    
    class ViewSection : public api::Section {
    public:
        explicit ViewSection(core::Evaluator& evaluator)
        : m_evaluator(evaluator)
        {}

        /// Add an already existing section to this view section - either at the specified offset, or at the end
        ///
        /// In case of overlapping spans, the span with the higher offset will truncate the span before. Changing the view
        /// once a span has been added is not implemented. Trying to add a view at the same offset will silently fail
        void addSectionSpan(u64 sectionId, u64 fromAddress, size_t size, std::optional<u64> atOffset = std::nullopt);
        
    protected:
        size_t size() const override;
        
        IOError resize(size_t) override;

        bool readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const override;
        
        IOError readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const override;

        IOError writeRaw(u64 toAddress, size_t size, ChunkWriter& writer) override;
        
    private:
        struct SectionSpan {
            u64 sectionId;
            u64 offset;
            size_t size;
        };

        /// Generalized iterator above underlying storage, handler signatures are:
        ///  - `bool stop = unmapped(u64 address, size_t chunkSize)`
        ///  - `bool stop = mapped(u64 address, size_t chunkSize, u64 sectionId, u64 chunkOffset)`
        ///
        /// `iterate` will visit all spans which overlap the provided area
        ///
        /// \returns Whether iteration was interrupted by a handler
        bool iterate(u64 fromAddress, size_t size, auto& unmapped, auto& mapped) const;
        
        template<bool IsRead>
        IOError access(u64 address, size_t size, auto& readerOrWriter) const;
        
        core::Evaluator& m_evaluator;
        std::map<u64, SectionSpan> m_spans;
        
        mutable bool m_isBeingAccessed = false;
        mutable bool m_isBeingInspected = false;
    };

}
