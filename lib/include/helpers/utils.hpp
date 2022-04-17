#pragma once

#include <helpers/types.hpp>

#include "concepts.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <cctype>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace pl {

    [[noreturn]] inline void unreachable() {
        __builtin_unreachable();
    }

    inline void unused(auto && ... x) {
        ((void)x, ...);
    }


    std::string to_string(u128 value);
    std::string to_string(i128 value);

    std::string encodeByteString(const std::vector<u8> &bytes);

    [[nodiscard]] constexpr inline u64 extract(u8 from, u8 to, const pl::unsigned_integral auto &value) {
        if (from < to) std::swap(from, to);

        using ValueType = std::remove_cvref_t<decltype(value)>;
        ValueType mask  = (std::numeric_limits<ValueType>::max() >> (((sizeof(value) * 8) - 1) - (from - to))) << to;

        return (value & mask) >> to;
    }

    [[nodiscard]] inline u64 extract(u32 from, u32 to, const std::vector<u8> &bytes) {
        u8 index = 0;
        while (from > 32 && to > 32) {
            from -= 8;
            to -= 8;
            index++;
        }

        u64 value = 0;
        std::memcpy(&value, &bytes[index], std::min(sizeof(value), bytes.size() - index));
        u64 mask = (std::numeric_limits<u64>::max() >> (64 - (from + 1)));

        return (value & mask) >> to;
    }

    constexpr inline i128 signExtend(size_t numBits, i128 value) {
        i128 mask = 1U << (numBits - 1);
        return (value ^ mask) - mask;
    }

    template<class... Ts>
    struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    template<size_t Size>
    struct SizeTypeImpl { };

    template<>
    struct SizeTypeImpl<1> { using Type = u8; };
    template<>
    struct SizeTypeImpl<2> { using Type = u16; };
    template<>
    struct SizeTypeImpl<4> { using Type = u32; };
    template<>
    struct SizeTypeImpl<8> { using Type = u64; };
    template<>
    struct SizeTypeImpl<16> { using Type = u128; };

    template<size_t Size>
    using SizeType = typename SizeTypeImpl<Size>::Type;

    template<typename T>
    constexpr T changeEndianess(const T &value, std::endian endian) {
        if (endian == std::endian::native)
            return value;

        constexpr auto Size = sizeof(T);

        SizeType<Size> unswapped;
        std::memcpy(&unswapped, &value, Size);

        SizeType<Size> swapped;

        if constexpr (!std::has_single_bit(Size) || Size > 16)
            static_assert(always_false<T>::value, "Invalid type provided!");

        switch (Size) {
            case 1:
                swapped = unswapped;
                break;
            case 2:
                swapped = __builtin_bswap16(unswapped);
                break;
            case 4:
                swapped = __builtin_bswap32(unswapped);
                break;
            case 8:
                swapped = __builtin_bswap64(unswapped);
                break;
            case 16:
                swapped = (u128(__builtin_bswap64(unswapped & 0xFFFF'FFFF'FFFF'FFFF)) << 64) | __builtin_bswap64(u128(unswapped) >> 64);
                break;
            default:
                pl::unreachable();
        }

        T result;
        std::memcpy(&result, &swapped, Size);

        return result;
    }

    [[nodiscard]] constexpr u128 bitmask(u8 bits) {
        return u128(-1) >> (128 - bits);
    }

    template<typename T>
    constexpr T changeEndianess(T value, size_t size, std::endian endian) {
        if (endian == std::endian::native)
            return value;

        u128 unswapped = 0;
        std::memcpy(&unswapped, &value, size);

        u128 swapped;

        switch (size) {
            case 1:
                swapped = unswapped;
                break;
            case 2:
                swapped = __builtin_bswap16(unswapped);
                break;
            case 4:
                swapped = __builtin_bswap32(unswapped);
                break;
            case 8:
                swapped = __builtin_bswap64(unswapped);
                break;
            case 16:
                swapped = (u128(__builtin_bswap64(unswapped & 0xFFFF'FFFF'FFFF'FFFF)) << 64) | __builtin_bswap64(u128(unswapped) >> 64);
                break;
            default:
                pl::unreachable();
        }

        T result = 0;
        std::memcpy(&result, &swapped, size);

        return result;
    }

    template<typename T, typename... Args>
    void moveToVector(std::vector<T> &buffer, T &&first, Args &&...rest) {
        buffer.push_back(std::move(first));

        if constexpr (sizeof...(rest) > 0)
            moveToVector(buffer, std::move(rest)...);
    }

    template<typename T, typename... Args>
    std::vector<T> moveToVector(T &&first, Args &&...rest) {
        std::vector<T> result;
        moveToVector(result, T(std::move(first)), std::move(rest)...);

        return result;
    }

    inline void trimLeft(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch) && ch >= 0x20;
        }));
    }

    inline void trimRight(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                    return !std::isspace(ch) && ch >= 0x20;
                }).base(),
                s.end());
    }

    inline void trim(std::string &s) {
        trimLeft(s);
        trimRight(s);
    }

    float float16ToFloat32(u16 float16);

    inline bool containsIgnoreCase(const std::string &a, const std::string &b) {
        auto iter = std::search(a.begin(), a.end(), b.begin(), b.end(), [](char ch1, char ch2) {
            return std::toupper(ch1) == std::toupper(ch2);
        });

        return iter != a.end();
    }
}