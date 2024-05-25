#include <pl/core/sections.hpp>

#include <pl/core/evaluator.hpp>

#include <fmt/format.h>

namespace pl::core {

    bool ZerosSection::readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const {
        ChunkAttributes attribs {
            .type = ChunkAttributes::Type::Zeros,
            .baseAddress = 0,
            .size = m_size
        };
        if (bool stop = reader(attribs)) {
            return true;
        }

        if (fromAddress + size < attribs.size) {
            return false;
        }

        return reader(hlp::restIsUnmapped(attribs));
    }

    auto ZerosSection::readRaw(u64, size_t size, ChunkReader& reader) const -> IOError {
        static constexpr u8 kZeros[4096] = {};
        
        while (size_t chunkSize = std::min(size, sizeof(kZeros))) {
            reader(std::span(kZeros).subspan(0, chunkSize));
            size -= chunkSize;
        }
        return std::nullopt;
    }

    DataSourceSection::DataSourceSection(size_t readBufferSize, size_t writeBufferSize)
    : m_readBuffer(readBufferSize)
    , m_writeBuffer(writeBufferSize)
    {}

    bool DataSourceSection::readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const {
        ChunkAttributes attribs {
            .type = ChunkAttributes::Type::Generic,
            .baseAddress = 0,
            .size = m_dataSize
        };
        if (bool stop = reader(attribs)) {
            return true;
        }
        
        if (fromAddress + size < attribs.size) {
            return false;
        }

        return reader(hlp::restIsUnmapped(attribs));
    }

    auto DataSourceSection::readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const -> IOError {
        if (!m_reader) {
            return "No memory has been attached. Reading is disabled";
        }
        if (m_readBufferInUse) {
            return "Reentrant read operations are not supported";
        }
        if (m_readBuffer.size() == 0) {
            return "Zero size read buffer prevents reading";
        }
        
        m_readBufferInUse = true;
        ON_SCOPE_EXIT { m_readBufferInUse = false; };
        
        while(size_t chunkSize = std::min(size, m_readBuffer.size())) {
            std::span<u8> span = std::span(m_readBuffer).subspan(0, chunkSize);
            
            m_reader(fromAddress, span.data(), span.size());
            
            if (auto error = reader(span)) {
                return error;
            }
            
            fromAddress += chunkSize;
            size -= chunkSize;
        }
        
        return std::nullopt;
    }

    auto DataSourceSection::writeRaw(u64 toAddress, size_t size, ChunkWriter& writer) -> IOError {
        if (!m_writer) {
            return "No memory has been attached. Writing is disabled";
        }
        if (m_readBufferInUse) {
            return "Reentrant write operations are not supported";
        }
        if (m_readBuffer.size() == 0) {
            return "Zero size write buffer prevents writing";
        }
        
        m_writeBufferInUse = true;
        ON_SCOPE_EXIT { m_writeBufferInUse = false; };
        
        while(size_t chunkSize = std::min(size, m_writeBuffer.size())) {
            std::span<u8> span = std::span(m_writeBuffer).subspan(0, chunkSize);
            
            if (auto error = writer(span)) {
                return error;
            }
            
            m_writer(toAddress, span.data(), span.size());
            
            toAddress += chunkSize;
            size -= chunkSize;
        }
        
        return std::nullopt;
    }

    void ViewSection::addSectionSpan(u64 sectionId, u64 fromAddress, size_t size, std::optional<u64> atOffset) {
        if (atOffset == std::nullopt) {
            if (m_spans.empty()) {
                atOffset = 0;
            } else {
                auto last = std::prev(m_spans.end());
                atOffset = last->first + last->second.size;
            }
        }
        
        m_spans.emplace(*atOffset, SectionSpan{
            .sectionId = sectionId,
            .offset = fromAddress,
            .size = size
        });
    }

    size_t ViewSection::size() const {
        if (m_spans.empty()) {
            return 0;
        }
        
        auto last = std::prev(m_spans.end());
        return (last->first + last->second.size) - m_spans.begin()->first;
    }

    auto ViewSection::resize(size_t) -> IOError {
        return "Not implemented";
    }

    bool ViewSection::readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const {
        if (m_isBeingInspected) {
            ChunkAttributes attribs {
                .type = ChunkAttributes::Type::Unmapped,
                .baseAddress = fromAddress,
                .size = size
            };
            
            return reader(attribs);
        }
        
        m_isBeingInspected = true;
        ON_SCOPE_EXIT { m_isBeingInspected = false; };
        
        ChunkAttributesReader remapper = [&reader](const ChunkAttributes& chunkAttribs) -> bool {
            ChunkAttributes attribs = chunkAttribs;
            
            return reader(attribs);
        };
        
        // This handler is called for unmapped ranges overlapping the query
        const auto unmappedHandler = [&reader](u64 from, size_t chunkSize) -> bool {
            ChunkAttributes attribs {
                .type = ChunkAttributes::Type::Unmapped,
                .baseAddress = from,
                .size = chunkSize
            };
            
            return reader(attribs);
        };
        
        // This handler is called for mapped ranges overlapping the query
        const auto mappedHandler = [this, &reader, &remapper](u64 address, size_t chunkSize, u64 sectionId, u64 chunkOffset) -> bool {
            try {
                return m_evaluator.getSection(sectionId).readChunkAttributes(chunkOffset, chunkSize, remapper);
            } catch (...) {
                ChunkAttributes attribs {
                    .type = ChunkAttributes::Type::Unmapped,
                    .baseAddress = address,
                    .size = chunkSize
                };
                return reader(attribs);
            }
        };
        
        return iterate(fromAddress, size, unmappedHandler, mappedHandler);
    }

    auto ViewSection::readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const -> IOError {
        return access<true>(fromAddress, size, reader);
    }

    auto ViewSection::writeRaw(u64 toAddress, size_t size, ChunkWriter& writer) -> IOError {
        return access<false>(toAddress, size, writer);
    }

    bool ViewSection::iterate(u64 fromAddress, size_t size, auto& unmapped, auto& mapped) const {
        if (m_spans.empty()) {
            return unmapped(0, std::numeric_limits<size_t>::max());
        }

        auto address = fromAddress;
        auto remain = size;
        
        auto next = m_spans.upper_bound(address);
        auto curr = std::prev(next);
        
        for (; curr != m_spans.end(); curr = next++) {
            if (address < curr->first) {
                auto size = curr->first - address;
                
                if (bool stop = unmapped(address, size)) {
                    return true;
                }
                
                address = curr->first;
                remain -= size;
            }
        
            if (remain == 0) {
                return false;
            }
            
            auto chunkBase = address - curr->first + curr->second.offset;
            auto chunkSize = std::min(remain, curr->second.size);
            
            if (next != m_spans.end()) {
                chunkSize = std::min(chunkSize, (size_t) next->first);
            }
            
            if (bool stop = mapped(address, chunkSize, curr->second.sectionId, chunkBase)) {
                return true;
            }
            
            address += chunkSize;
            remain -= chunkSize;
            
            if (remain == 0) {
                return false;
            }
        }
        
        return unmapped(address, std::numeric_limits<size_t>::max() - address);
    }

    template<bool IsRead>
    auto ViewSection::access(u64 fromAddress, size_t size, auto& readerOrWriter) const -> IOError {
        if (m_isBeingAccessed) {
            return "View recursion not permitted";
        }
        
        m_isBeingAccessed = true;
        ON_SCOPE_EXIT { m_isBeingAccessed = false; };
        
        IOError result;
        
        const auto fail = [&result](u64 from, u64 to, const std::string& extra) {
            result = fmt::format("Attempted to access out-of-bounds area 0x{:X}-0x{:X} (of {} bytes). {}", from, to, to - from, extra);
            return true;
        };

        // This handler is called for unmapped ranges overlapping the query
        const auto unmappedHandler = [this, &fail, fromAddress, size](u64 from, size_t chunkSize) -> bool {
            u64 to = std::min(from + chunkSize, fromAddress + size);
            
            std::string hint;
            
            if (m_spans.empty()) {
                hint.append("ViewSection is empty!");
            } else {
                auto after = m_spans.upper_bound(from);
                
                auto before = std::prev(after);
                if (from < before->first) {
                    auto endsAt = before->first + before->second.size;
                    
                    hint.append(fmt::format("Last mapped area before ends at 0x{:X} ({} bytes away).", endsAt, to - endsAt));
                }

                if (after != m_spans.end()) {
                    auto startsAt = after->first;
                    
                    hint.append((hint.size() == 0) ? "" : " ");
                    hint.append(fmt::format("First mapped area after starts at 0x{:X} ({} bytes away).", startsAt, startsAt - from));
                }
            }

            return fail(from, to, hint);
        };
        
        // This handler is called for mapped ranges overlapping the query
        const auto mappedHandler = [this, &fail, &result, &readerOrWriter](u64 address, size_t chunkSize, u64 sectionId, u64 chunkOffset) -> bool {
            try {
                auto& section = m_evaluator.getSection(sectionId);
                
                if constexpr (IsRead) {
                    auto error = section.read(chunkOffset, chunkSize, readerOrWriter);
                    if (!error) {
                        return false; // Continue
                    }
                    
                    result = fmt::format("Error writing underlying section {}: {}", sectionId, *error);
                    return true; // Stop
                } else {
                    auto error = section.write(false, chunkOffset, chunkSize, readerOrWriter);
                    if (!error) {
                        return false; // Continue;
                    }
                    
                    result = fmt::format("Error writing underlying section {}: {}", sectionId, *error);
                    return true; // Stop
                }
            } catch (...) {
                return fail(address, address + chunkSize, fmt::format("Failed to access mapped section {}", sectionId));
            }
        };
        
        iterate(fromAddress, size, unmappedHandler, mappedHandler);
        return result;
    }

}
