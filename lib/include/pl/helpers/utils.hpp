#pragma once

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

#include <pl/helpers/types.hpp>
#include <pl/helpers/concepts.hpp>

#include <wolv/utils/core.hpp>

namespace pl::hlp {

    [[nodiscard]] std::string to_string(u128 value);
    [[nodiscard]] std::string to_string(i128 value);

    [[nodiscard]] std::vector<u8> toMinimalBytes(const auto &value) {
        auto bytesArray = wolv::util::toBytes(value);
        auto bytes = std::vector<u8>(bytesArray.begin(), bytesArray.end());

        while (bytes.size() > 1 && bytes.back() == 0)
            bytes.pop_back();

        if (bytes.size() < sizeof(u128)) {
            u32 newSize;
            for (newSize = 1; newSize < bytes.size(); newSize <<= 1);

            bytes.resize(newSize);
        }

        return bytes;
    }

    std::string encodeByteString(const std::vector<u8> &bytes);

    [[nodiscard]] constexpr inline i128 signExtend(size_t numBits, i128 value) {
        i128 mask = u128(1) << u128(numBits - 1);
        return (value ^ mask) - mask;
    }

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
    [[nodiscard]] constexpr T changeEndianess(const T &value, size_t size, std::endian endian) {
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
    [[nodiscard]] constexpr T changeEndianess(const T &value, std::endian endian) {
        return changeEndianess(value, sizeof(value), endian);
    }

    template<typename T, typename... Args>
    void moveToVector(std::vector<T> & buffer, Args &&...rest) {
        buffer.reserve(sizeof...(rest));
        (buffer.push_back(std::move(rest)),...);
    }

    template<typename T, typename... Args>
    [[nodiscard]] std::vector<T> moveToVector(T &&first, Args &&...rest) {
        std::vector<T> result;
        moveToVector(result, std::move(first), std::move(rest)...);

        return result;
    }

    [[nodiscard]] float float16ToFloat32(u16 float16);

    [[nodiscard]] inline bool containsIgnoreCase(const std::string &a, const std::string &b) {
        auto iter = std::search(a.begin(), a.end(), b.begin(), b.end(), [](char ch1, char ch2) {
            return std::toupper(ch1) == std::toupper(ch2);
        });

        return iter != a.end();
    }

}