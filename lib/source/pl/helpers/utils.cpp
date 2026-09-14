#include <pl/helpers/utils.hpp>

#include <fmt/format.h>
#include <wolv/hash/crc.hpp>

namespace pl::hlp {

#if defined(LIBWOLV_BUILTIN_UINT128)
    std::string to_string(u128 value) {
        return ::fmt::format("{}", value);
    }

    std::string to_string(i128 value) {
        return ::fmt::format("{}", value);
    }
#else
    std::string to_string(u128 value) {
        return math::wide_integer::to_string(value);
    }

    std::string to_string(i128 value) {
        return math::wide_integer::to_string(value);
    }

    std::string to_hex_string(u128 value) {
        (void)value;
        return "TODO";
    }

    std::string to_hex_string(i128 value) {
        (void)value;
        return "TODO";
    }
#endif // LIBWOLV_BUILTIN_UINT128

    std::vector<u8> decodeByteString(const std::string &str) {
        std::vector<u8> result;

        for (size_t i = 0; i < str.size(); i++)
            result.push_back(str[i]);

        return result;
    }

    Utf8Codepoint decodeUtf8Codepoint(std::string_view text) {
        if (text.empty())
            return { };

        const u8 lead = u8(text[0]);

        size_t length = 0;
        u32 value = 0;
        if (lead < 0x80)                                 { length = 1; value = lead; }
        else if ((lead & 0xE0) == 0xC0 && lead >= 0xC2)  { length = 2; value = lead & 0x1F; }
        else if ((lead & 0xF0) == 0xE0)                  { length = 3; value = lead & 0x0F; }
        else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4)  { length = 4; value = lead & 0x07; }
        else
            return { };

        if (text.size() < length)
            return { };

        for (size_t i = 1; i < length; i += 1) {
            const u8 continuation = u8(text[i]);
            if ((continuation & 0xC0) != 0x80)
                return { };

            value = (value << 6) | (continuation & 0x3F);
        }

        // An overlong sequence, a surrogate, or a value past the codespace is
        // not a code point, whatever the lead byte promised.
        // A two byte overlong needs no test of its own. The lead byte test
        // above demands 0xC2 or more, so the value is always 0x80 or more.
        if (length == 3 && value < 0x800)           return { };
        if (length == 4 && value < 0x10000)         return { };
        if (value >= 0xD800 && value <= 0xDFFF)     return { };
        if (value > 0x10FFFF)                       return { };

        return { value, length };
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

    u32 stringCrc32(const std::string &str) {
        wolv::hash::Crc<32> crc32(0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, true);

        crc32.process(decodeByteString(str));

        return crc32.getResult();
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