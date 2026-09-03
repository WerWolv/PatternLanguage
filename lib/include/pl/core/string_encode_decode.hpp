#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pl/helpers/types.hpp>

namespace pl::core {

    // A pluggable text codec for string patterns. The host application sets this on the
    // Evaluator. With none set, string patterns keep their old raw-byte behavior.
    //
    // decode() and encode() are fallible: std::nullopt when `bytes`/`text` is not
    // valid or representable under the named encoding. encodeLossy() never fails,
    // substituting a replacement for anything it cannot represent.
    class StringEncodeDecode {
    public:
        virtual ~StringEncodeDecode() = default;

        // `encoding` is empty when the pattern names no encoding; the codec picks
        // its own default then.
        [[nodiscard]] virtual std::optional<std::string> decode(std::span<const u8> bytes, std::string_view encoding) const = 0;
        [[nodiscard]] virtual std::optional<std::vector<u8>> encode(std::string_view text, std::string_view encoding) const = 0;

        [[nodiscard]] virtual std::vector<u8> encodeLossy(std::string_view text, std::string_view encoding) const = 0;
    };

}
