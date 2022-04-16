#include "helpers/utils.hpp"

#include <cstdio>
#include <codecvt>
#include <locale>
#include <filesystem>

#include "fmt/format.h"

namespace pl {

    std::string to_string(u128 value) {
        char data[45] = {0};

        u8 index = sizeof(data) - 2;
        while (value != 0 && index != 0) {
            data[index] = '0' + value % 10;
            value /= 10;
            index--;
        }

        return {data + index + 1};
    }

    std::string to_string(i128 value) {
        char data[45] = {0};

        u128 unsignedValue = value < 0 ? -value : value;

        u8 index = sizeof(data) - 2;
        while (unsignedValue != 0 && index != 0) {
            data[index] = '0' + unsignedValue % 10;
            unsignedValue /= 10;
            index--;
        }

        if (value < 0) {
            data[index] = '-';
            return {data + index};
        } else
            return {data + index + 1};
    }

    std::string toByteString(u64 bytes) {
        double value = bytes;
        u8 unitIndex = 0;

        while (value > 1024) {
            value /= 1024;
            unitIndex++;

            if (unitIndex == 6)
                break;
        }

        std::string result = fmt::format("{0:.2f}", value);

        switch (unitIndex) {
            case 0:
                result += " Bytes";
                break;
            case 1:
                result += " kB";
                break;
            case 2:
                result += " MB";
                break;
            case 3:
                result += " GB";
                break;
            case 4:
                result += " TB";
                break;
            case 5:
                result += " PB";
                break;
            case 6:
                result += " EB";
                break;
            default:
                result = "A lot!";
        }

        return result;
    }

    std::string makeStringPrintable(const std::string &string) {
        std::string result;
        for (char c: string) {
            if (std::isprint(c))
                result += c;
            else
                result += fmt::format("\\x{0:02X}", u8(c));
        }

        return result;
    }

    std::string makePrintable(u8 c) {
        switch (c) {
            case 0:
                return "NUL";
            case 1:
                return "SOH";
            case 2:
                return "STX";
            case 3:
                return "ETX";
            case 4:
                return "EOT";
            case 5:
                return "ENQ";
            case 6:
                return "ACK";
            case 7:
                return "BEL";
            case 8:
                return "BS";
            case 9:
                return "TAB";
            case 10:
                return "LF";
            case 11:
                return "VT";
            case 12:
                return "FF";
            case 13:
                return "CR";
            case 14:
                return "SO";
            case 15:
                return "SI";
            case 16:
                return "DLE";
            case 17:
                return "DC1";
            case 18:
                return "DC2";
            case 19:
                return "DC3";
            case 20:
                return "DC4";
            case 21:
                return "NAK";
            case 22:
                return "SYN";
            case 23:
                return "ETB";
            case 24:
                return "CAN";
            case 25:
                return "EM";
            case 26:
                return "SUB";
            case 27:
                return "ESC";
            case 28:
                return "FS";
            case 29:
                return "GS";
            case 30:
                return "RS";
            case 31:
                return "US";
            case 32:
                return "Space";
            case 127:
                return "DEL";
            case 128 ... 255:
                return " ";
            default:
                return std::string() + static_cast<char>(c);
        }
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

    std::vector<u8> decodeByteString(const std::string &string) {
        u32 offset = 0;
        std::vector<u8> result;

        while (offset < string.length()) {
            auto c = [&] { return string[offset]; };

            if (c() == '\\') {
                if ((offset + 2) >= string.length()) return {};

                offset++;

                char escapeChar = c();

                offset++;

                switch (escapeChar) {
                    case 'a':
                        result.push_back('\a');
                        break;
                    case 'b':
                        result.push_back('\b');
                        break;
                    case 'f':
                        result.push_back('\f');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    case 'v':
                        result.push_back('\v');
                        break;
                    case '\\':
                        result.push_back('\\');
                        break;
                    case 'x': {
                        u8 byte = 0x00;
                        if ((offset + 1) >= string.length()) return {};

                        for (u8 i = 0; i < 2; i++) {
                            byte <<= 4;
                            if (c() >= '0' && c() <= '9')
                                byte |= 0x00 + (c() - '0');
                            else if (c() >= 'A' && c() <= 'F')
                                byte |= 0x0A + (c() - 'A');
                            else if (c() >= 'a' && c() <= 'f')
                                byte |= 0x0A + (c() - 'a');
                            else
                                return {};

                            offset++;
                        }

                        result.push_back(byte);
                    }
                        break;
                    default:
                        return {};
                }
            } else {
                result.push_back(c());
                offset++;
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