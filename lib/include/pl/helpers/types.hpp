#pragma once

#include <cstddef>
#include <cstdint>
#include <wolv/types.hpp>

namespace pl {

    using u8   = std::uint8_t;
    using u16  = std::uint16_t;
    using u32  = std::uint32_t;
    using u64  = std::uint64_t;
    using u128 = wolv::u128;

    using i8   = std::int8_t;
    using i16  = std::int16_t;
    using i32  = std::int32_t;
    using i64  = std::int64_t;
    using i128 = wolv::i128;

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

#if !defined(LIBWOLV_BUILTIN_UINT128)

#include <string>
#include <fmt/format.h>

namespace pl::hlp
{
    [[nodiscard]] std::string to_string(u128 value);
    [[nodiscard]] std::string to_string(i128 value);
    [[nodiscard]] std::string to_hex_string(u128 value);
    [[nodiscard]] std::string to_hex_string(i128 value);
}

template<>
struct fmt::formatter<pl::u128>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return fmt::formatter<uint64_t>().parse(ctx);
    }

    auto format(const pl::u128& v, format_context& ctx) const {
        return format_to(ctx.out(), "u128({})", pl::hlp::to_string(v));
    }
};

template<>
struct fmt::formatter<pl::i128>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return fmt::formatter<int64_t>().parse(ctx);
    }

    auto format(const pl::i128& v, format_context& ctx) const {
        return format_to(ctx.out(), "i128({})", pl::hlp::to_string(v));
    }
};
#endif
