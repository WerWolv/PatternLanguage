#pragma once

#include <cstddef>
#include <cstdint>
#include "uintwide_t.h"

namespace pl {

    using u8   = std::uint8_t;
    using u16  = std::uint16_t;
    using u32  = std::uint32_t;
    using u64  = std::uint64_t;
    using u128 = math::wide_integer::uint128_t;

    using i8   = std::int8_t;
    using i16  = std::int16_t;
    using i32  = std::int32_t;
    using i64  = std::int64_t;
    using i128 = math::wide_integer::int128_t;

    struct Region {
        u64 address;
        u64 size;
    };

    enum class ControlFlowStatement {
        None,
        Continue,
        Break,
        Return
    };

}

#if 0
#include <fmt/format.h>

// Reference: https://github.com/nd-nuclear-theory/am/blob/a7c3defccc64b8d04b308adfd57824eacd988e6a/halfint_fmt.h
namespace fmt {

    template<const size_t Width2,
        typename LimbType,
        typename AllocatorType,
        const bool IsSigned>
    struct formatter<math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> : fmt::formatter<std::string> {
        char presentation = 'g';

        template <typename ParseContext>
        FMT_CONSTEXPR auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
            auto it = ctx.begin(), end = ctx.end();
            if (it != end && (*it == 'd' || *it == 'f' || *it == 'g')) presentation = *it++;

            // Check if reached the end of the range:
            if (it != end && *it != '}')
                throw format_error("invalid format");

            // Return an iterator past the end of the parsed range:
            return it;
        }

        template <typename FormatContext>
        FMT_CONSTEXPR auto format(const math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& n, FormatContext& ctx) const -> decltype(ctx.out()) {
            return fmt::formatter<std::string>::format(math::wide_integer::to_string(n), ctx);
        }
    };

}  // namespace fmt

template<> struct fmt::formatter<pl::u128> : fmt::formatter<std::string> {
    fmt::format_context::iterator format(const pl::u128& n, fmt::format_context& ctx) const {
        return fmt::formatter<std::string>::format(math::wide_integer::to_string(n), ctx);
    }
};

template<> struct fmt::formatter<pl::i128> : fmt::formatter<std::string> {
    fmt::format_context::iterator format(const pl::i128& n, fmt::format_context& ctx) const {
        return fmt::formatter<std::string>::format(math::wide_integer::to_string(n), ctx);
    }
};
#endif