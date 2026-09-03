#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pl/helpers/types.hpp>

namespace pl::core {

    /// Why decode() stopped before the end of `bytes`.
    enum class DecodeStop {
        EndOfInput,      ///< Consumed every byte given
        CodepointLimit,  ///< Reached maxCodepoints; more input may remain
        MalformedBytes,  ///< The byte sequence at bytesConsumed is not valid under this encoding
    };

    struct DecodeResult {
        std::string text;                              ///< Whole code points only, never a mid-sequence fragment
        size_t bytesConsumed = 0;                       ///< How much of the input `text` accounts for
        size_t codepointCount = 0;                      ///< Code points in `text`
        DecodeStop stopReason = DecodeStop::EndOfInput;
    };

    /**
     * @brief A pluggable text codec for string patterns
     * @note The host application sets this on the Evaluator. With none set, string
     * patterns keep their old raw-byte behavior.
     */
    class StringEncodeDecode {
    public:
        virtual ~StringEncodeDecode() = default;

        /**
         * @brief Decodes `bytes` under `encoding`, one code point at a time
         * @param bytes Bytes to decode
         * @param encoding Encoding to decode with; empty when the pattern names no
         * encoding, in which case the codec picks its own default
         * @param maxCodepoints Stop after decoding this many code points. std::nullopt
         * decodes as much of `bytes` as is valid, with no cap.
         * @return Code points decoded up to the first malformed byte, the end of
         * `bytes`, or maxCodepoints - whichever comes first. `stopReason` says which.
         */
        [[nodiscard]] virtual DecodeResult decode(std::span<const u8> bytes, std::string_view encoding,
            std::optional<size_t> maxCodepoints = std::nullopt) const = 0;

        /**
         * @brief Encodes `text` under `encoding`
         * @param text Text to encode
         * @param encoding Encoding to encode with; empty when the pattern names no
         * encoding, in which case the codec picks its own default
         * @return The encoded bytes, or std::nullopt when `text` is not representable
         * under `encoding`
         */
        [[nodiscard]] virtual std::optional<std::vector<u8>> encode(std::string_view text, std::string_view encoding) const = 0;

        /**
         * @brief Encodes `text` under `encoding`, never failing
         * @param text Text to encode
         * @param encoding Encoding to encode with; empty when the pattern names no
         * encoding, in which case the codec picks its own default
         * @return The encoded bytes, substituting a replacement for anything `encoding`
         * cannot represent
         */
        [[nodiscard]] virtual std::vector<u8> encodeLossy(std::string_view text, std::string_view encoding) const = 0;
    };

}
