#pragma once

#include <pl/core/token.hpp>
#include <pl/core/errors/result.hpp>
#include <pl/helpers/types.hpp>

#include <cmath>
#include <vector>
#include <functional>
#include <string>
#include <optional>
#include <span>

namespace pl {

    class PatternLanguage;

    namespace core {
        class Evaluator;
        class Preprocessor;
    }

}

namespace pl::api {

    /**
     * @brief A pragma handler is a function that is called when a pragma is encountered.
     * @param string& Value that was set for the pragma
     * @return true if the value parameter was a valid value for the pragma, else false
     */
    using PragmaHandler = std::function<bool(PatternLanguage&, const std::string &)>;

    using DirectiveHandler = std::function<void(core::Preprocessor*, u32)>;

    using Resolver = std::function<hlp::Result<Source*, std::string>(const std::string&)>;

    struct Source {
        std::string content;
        std::string source;
        u32 id = 0;

        static u32 idCounter;

        Source(std::string content, std::string source = DefaultSource) :
            content(std::move(content)), source(std::move(source)), id(idCounter++) { }

        Source() = default;

        static constexpr auto DefaultSource = "<Source Code>";
        static constexpr Source* NoSource = nullptr;
        static Source Empty() {
            return { "", "" };
        }

        constexpr auto operator<=>(const Source& other) const {
            return this->id <=> other.id;
        }

    };

    class Section {
    public:
        using IOError = std::optional<std::string>;
        
        using ChunkReader = std::function<IOError(std::span<const u8>)>;
        using ChunkWriter = std::function<IOError(std::span<u8>)>;

        virtual ~Section() {};
        
        /// Read data from this section into the provided contiguous buffer
        IOError read(u64 fromAddress, std::span<u8> into) const;

        /// Read data from this section - the section will call `reader` with the largest contiguous chunks it can provide
        IOError read(u64 fromAddress, size_t size, ChunkReader& reader) const;
        
        /// Write data to this section from the provided contiguous buffer
        IOError write(bool expand, u64 toAddress, std::span<const u8> from);

        /// Write data to this section - the section will call `writer` with the largest contiguous chunks it can provide
        IOError write(bool expand, u64 toAddress, size_t size, ChunkWriter& writer);

        /// Write data to this section from the provided section
        IOError write(bool expand, u64 toAddress, size_t size, u64 fromAddress, api::Section& fromSection);

        /// Access the size of this section
        virtual size_t size() const = 0;
        
        /// Shrink or expand this section to the specified size
        virtual IOError resize(size_t newSize) = 0;

        struct ChunkAttributes {
            enum class Type {
                /// Represents a chunk of unknown quality - returned when the section implementation
                /// would need to fetch the data to reason about it's attributes, or does not support
                /// chunk attribute access
                Unknown,
                
                /// Represents a chunk of address space with normal data inside
                Generic,
                
                /// Represents a chunk of address space with no data inside
                Unmapped,
                
                /// Represents a chunk of address space with nothing but zeros - note that section implementations
                /// are not required to detect such repeating sections
                Zeros,
            };
            
            /// Type of this chunk
            Type type = Type::Generic;
            
            /// Start address of this chunk
            u64 baseAddress = 0x00;
            
            /// Size of the chunk
            size_t size = 0;
            
            /// Whether the underlying bytes are writable
            bool writable = false;
        };
        using ChunkAttributesReader = std::function<bool(const ChunkAttributes&)>;
        
        /// Read all attributed chunks overlapping the specified area - sections are guaranteed to produce non-overlapping contiguous chunks in their
        /// address space, however the first/last chunks are not guaranteed to fit into the specified window.
        ///
        /// The provided `ChunkAttributesReader`  may return `true` to stop iterating chunk attributes.
        ///
        /// \returns Whether reading was interrupted by `reader`
        virtual bool readChunkAttributes(u64 fromAddress, size_t size, ChunkAttributesReader& reader) const = 0;
        
    protected:
        /// Performs a chunked write operation - incoming parameters have been already validated
        virtual IOError readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const = 0;

        /// Performs a chunked write operation - incoming parameters have been already validated
        virtual IOError writeRaw(u64 toAddress, size_t size, ChunkWriter& writer) = 0;
    };

    /**
     * @brief A type representing a custom section
     */
    struct CustomSection {
        std::string name;
        std::unique_ptr<Section> section;
    };

    /**
     * @brief Type to pass to function register functions to specify the number of parameters a function takes.
     */
    struct FunctionParameterCount {
        FunctionParameterCount() = default;

        constexpr bool operator==(const FunctionParameterCount &other) const {
            return this->min == other.min && this->max == other.max;
        }

        [[nodiscard]] static FunctionParameterCount unlimited() {
            return FunctionParameterCount { 0, 0xFFFF'FFFF };
        }

        [[nodiscard]] static FunctionParameterCount none() {
            return FunctionParameterCount { 0, 0 };
        }

        [[nodiscard]] static FunctionParameterCount exactly(u32 value) {
            return FunctionParameterCount { value, value };
        }

        [[nodiscard]] static FunctionParameterCount moreThan(u32 value) {
            return FunctionParameterCount { value + 1, 0xFFFF'FFFF };
        }

        [[nodiscard]] static FunctionParameterCount lessThan(u32 value) {
            return FunctionParameterCount { 0, u32(std::max<i64>(i64(value) - 1, 0)) };
        }

        [[nodiscard]] static FunctionParameterCount atLeast(u32 value) {
            return FunctionParameterCount { value, 0xFFFF'FFFF };
        }

        [[nodiscard]] static FunctionParameterCount between(u32 min, u32 max) {
            return FunctionParameterCount { min, max };
        }

        u32 min = 0, max = 0;
    private:
        FunctionParameterCount(u32 min, u32 max) : min(min), max(max) { }
    };

    /**
     * @brief A type representing a namespace.
     */
    using Namespace = std::vector<std::string>;

    /**
     * @brief A function callback called when a function is called.
     */
    using FunctionCallback  = std::function<std::optional<core::Token::Literal>(core::Evaluator *, const std::vector<core::Token::Literal> &)>;

    /**
     * @brief A type representing a function.
     */
    struct Function {
        FunctionParameterCount parameterCount;
        std::vector<core::Token::Literal> defaultParameters;
        FunctionCallback func;
        bool dangerous;
    };

}
