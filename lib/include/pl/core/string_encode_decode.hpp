#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pl/helpers/types.hpp>

namespace pl::core {

    /**
     * @brief A pluggable text codec for string patterns
     * @note The host application sets this on the Evaluator. With none set, string
     * patterns keep their old raw-byte behavior.
     */
    class StringEncodeDecode {
    public:
        virtual ~StringEncodeDecode() = default;

        /**
         * @brief Decodes `bytes` under `encoding`
         * @param bytes Bytes to decode
         * @param encoding Encoding to decode with; empty when the pattern names no
         * encoding, in which case the codec picks its own default
         * @return The decoded text, or std::nullopt when `bytes` is not valid under
         * `encoding`
         */
        [[nodiscard]] virtual std::optional<std::string> decode(std::span<const u8> bytes, std::string_view encoding) const = 0;

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
