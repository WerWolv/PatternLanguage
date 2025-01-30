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

#include <fmt/format.h>

template<>
struct fmt::formatter<pl::u128>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return fmt::formatter<pl::u64>().parse(ctx);
    }

    auto format(const pl::u128& v, format_context& ctx) const {
        return fmt::formatter<pl::u64>().format(pl::u64(v), ctx);
    }
};

template<>
struct fmt::formatter<pl::i128>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return fmt::formatter<pl::u64>().parse(ctx);
    }

    auto format(const pl::i128& v, format_context& ctx) const {
        return fmt::formatter<pl::u64>().format(pl::i64(v), ctx);
    }
};
#endif
