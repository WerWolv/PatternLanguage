#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternUnicodeEscapes : public TestPattern {
    public:
        TestPatternUnicodeEscapes(core::Evaluator *evaluator) : TestPattern(evaluator, "UnicodeEscapes") {
        }
        ~TestPatternUnicodeEscapes() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #define MSG "Unicode escape failed"

                // \u and \U give a code point, encoded as UTF-8. \x gives one
                // byte. The two are only the same below U+0080.
                std::assert("\u0041" == "A", MSG);
                std::assert("\u0041" == "\x41", MSG);

                // Two bytes above U+007F. Before, \u kept the low byte only.
                std::assert("\u00E9" == "\xC3\xA9", MSG);
                std::assert("\u00E9" != "\xE9", MSG);

                // Three bytes. The high byte of the code point must survive.
                std::assert("\u20AC" == "\xE2\x82\xAC", MSG);
                std::assert("\u20AC" != "\xAC", MSG);

                // The last code point \u can reach.
                std::assert("\uFFFD" == "\xEF\xBF\xBD", MSG);

                // \U reads eight digits and reaches past U+FFFF.
                std::assert("\U00000041" == "A", MSG);
                std::assert("\U000000E9" == "\xC3\xA9", MSG);
                std::assert("\U0001F600" == "\xF0\x9F\x98\x80", MSG);
                std::assert("\U0010FFFF" == "\xF4\x8F\xBF\xBF", MSG);

                // An escape joins the text around it.
                std::assert("caf\u00E9" == "caf\xC3\xA9", MSG);
                std::assert("\u00E9\u00E9" == "\xC3\xA9\xC3\xA9", MSG);

                // A literal character in the source needs no escape.
                std::assert("café" == "caf\u00E9", MSG);

                // Lower case digits read the same as upper case ones.
                std::assert("\u00e9" == "\u00E9", MSG);
                std::assert("\xc3" == "\xC3", MSG);
                std::assert("\U0001f600" == "\U0001F600", MSG);

                // The other escapes still give one byte each.
                std::assert("\n" == "\x0A", MSG);
                std::assert("\t" == "\x09", MSG);
                std::assert("\r" == "\x0D", MSG);
                std::assert("\a" == "\x07", MSG);
                std::assert("\b" == "\x08", MSG);
                std::assert("\f" == "\x0C", MSG);
                std::assert("\0" == "\x00", MSG);
                std::assert("\'" == "\x27", MSG);
                std::assert("\\" == "\x5C", MSG);
                std::assert("\"" == "\x22", MSG);

                // A char still holds one byte.
                std::assert('A' == 0x41, MSG);
                std::assert('\x41' == 0x41, MSG);
                std::assert('\u0041' == 0x41, MSG);
                std::assert('\n' == 0x0A, MSG);
            )";
        }
    };

    // Each of these must fail to compile. One case per test, because the first
    // error stops the lexer.

    class TestPatternUnicodeEscapeSurrogateFail : public TestPattern {
    public:
        TestPatternUnicodeEscapeSurrogateFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "UnicodeEscapeSurrogateFail", Mode::Failing) { }
        ~TestPatternUnicodeEscapeSurrogateFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // A surrogate is not a code point, so it has no UTF-8 form.
            return R"(
                str s = "\uD800";
            )";
        }
    };

    class TestPatternUnicodeEscapeOutOfRangeFail : public TestPattern {
    public:
        TestPatternUnicodeEscapeOutOfRangeFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "UnicodeEscapeOutOfRangeFail", Mode::Failing) { }
        ~TestPatternUnicodeEscapeOutOfRangeFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // Past the last code point.
            return R"(
                str s = "\U00110000";
            )";
        }
    };

    class TestPatternUnicodeEscapeBadDigitFail : public TestPattern {
    public:
        TestPatternUnicodeEscapeBadDigitFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "UnicodeEscapeBadDigitFail", Mode::Failing) { }
        ~TestPatternUnicodeEscapeBadDigitFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // Not a hex digit.
            return R"(
                str s = "\u12Z4";
            )";
        }
    };

    class TestPatternUnicodeEscapeShortFail : public TestPattern {
    public:
        TestPatternUnicodeEscapeShortFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "UnicodeEscapeShortFail", Mode::Failing) { }
        ~TestPatternUnicodeEscapeShortFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // \u needs four digits. The string ends first.
            return R"(
                str s = "\u12";
            )";
        }
    };

    class TestPatternHexEscapeBadDigitFail : public TestPattern {
    public:
        TestPatternHexEscapeBadDigitFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "HexEscapeBadDigitFail", Mode::Failing) { }
        ~TestPatternHexEscapeBadDigitFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // Not a hex digit. std::stoul() accepted this before.
            return R"(
                str s = "\xZZ";
            )";
        }
    };

    class TestPatternHexEscapeShortFail : public TestPattern {
    public:
        TestPatternHexEscapeShortFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "HexEscapeShortFail", Mode::Failing) { }
        ~TestPatternHexEscapeShortFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // \x needs two digits. The string ends first.
            return R"(
                str s = "\x4";
            )";
        }
    };

    class TestPatternUnknownEscapeFail : public TestPattern {
    public:
        TestPatternUnknownEscapeFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "UnknownEscapeFail", Mode::Failing) { }
        ~TestPatternUnknownEscapeFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // No such escape.
            return R"(
                str s = "\q";
            )";
        }
    };

    class TestPatternUnicodeEscapeInCharFail : public TestPattern {
    public:
        TestPatternUnicodeEscapeInCharFail(core::Evaluator *evaluator)
            : TestPattern(evaluator, "UnicodeEscapeInCharFail", Mode::Failing) { }
        ~TestPatternUnicodeEscapeInCharFail() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            // A char holds one byte, and this code point needs two.
            return R"(
                char c = '\u00E9';
            )";
        }
    };

}