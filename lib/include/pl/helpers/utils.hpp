#pragma once

#include <pl/helpers/types.hpp>
#include <pl/helpers/concepts.hpp>

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

namespace pl::hlp {

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

    [[nodiscard]] constexpr u128 bitmask(u8 bits) {
        return u128(-1) >> (128 - bits);
    }

    template<typename T>
    constexpr T changeEndianess(const T &value, size_t size, std::endian endian) {
        if (endian == std::endian::native)
            return value;

        size = std::min(size, sizeof(T));

        std::array<uint8_t, 16> data = { 0 };
        std::memcpy(&data[0], &value, size);

        for (uint32_t i = 0; i < size / 2; i++) {
            std::swap(data[i], data[size - 1 - i]);
        }

        T result = 0x00;
        std::memcpy(&result, &data[0], size);

        return result;
    }

    template<pl::integral T>
    constexpr T changeEndianess(const T &value, std::endian endian) {
        return changeEndianess(value, sizeof(value), endian);
    }

    template<typename T, typename... Args>
    void moveToVector(std::vector<T> & buffer, Args &&...rest) {
        buffer.reserve(sizeof...(rest));
        (buffer.push_back(std::move(rest)),...);
    }

    template<typename T, typename... Args>
    std::vector<T> moveToVector(T &&first, Args &&...rest) {
        std::vector<T> result;
        moveToVector(result, std::move(first), std::move(rest)...);

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

    std::string replaceAll(std::string string, const std::string &search, const std::string &replace);
    std::vector<std::string> splitString(const std::string &string, const std::string &delimiter);

    constexpr inline size_t strnlen(const char *s, size_t n) {
        size_t i = 0;
        while (i < n && s[i] != '\x00') i++;

        return i;
    }

}