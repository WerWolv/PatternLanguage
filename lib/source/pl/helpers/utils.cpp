#include <pl/helpers/utils.hpp>

#include <codecvt>

#include <fmt/format.h>

namespace pl::hlp {

    std::string to_string(u128 value) {
        return fmt::format("{}", value);
    }

    std::string to_string(i128 value) {
        return fmt::format("{}", value);
    }

    std::string encodeByteString(const std::vector<u8> &bytes) {
        std::string result;

        for (u8 byte: bytes) {
            if (std::isprint(byte) && byte != '\\')
                result += char(byte);
            else {
                switch (byte) {
                    case '\\':
                        result += "\\";
                        break;
                    case '\a':
                        result += "\\a";
                        break;
                    case '\b':
                        result += "\\b";
                        break;
                    case '\f':
                        result += "\\f";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    case '\t':
                        result += "\\t";
                        break;
                    case '\v':
                        result += "\\v";
                        break;
                    default:
                        result += fmt::format("\\x{:02X}", byte);
                        break;
                }
            }
        }

        return result;
    }

    float float16ToFloat32(u16 float16) {
        u32 sign = float16 >> 15;
        u32 exponent = (float16 >> 10) & 0x1F;
        u32 mantissa = float16 & 0x3FF;

        u32 result = 0x00;

        if (exponent == 0) {
            if (mantissa == 0) {
                // +- Zero
                result = sign << 31;
            } else {
                // Subnormal value
                exponent = 0x7F - 14;

                while ((mantissa & (1 << 10)) == 0) {
                    exponent--;
                    mantissa <<= 1;
                }

                mantissa &= 0x3FF;
                result = (sign << 31) | (exponent << 23) | (mantissa << 13);
            }
        } else if (exponent == 0x1F) {
            // +-Inf or +-NaN
            result = (sign << 31) | (0xFF << 23) | (mantissa << 13);
        } else {
            // Normal value
            result = (sign << 31) | ((exponent + (0x7F - 15)) << 23) | (mantissa << 13);
        }

        float floatResult = 0;
        std::memcpy(&floatResult, &result, sizeof(float));

        return floatResult;
    }
}