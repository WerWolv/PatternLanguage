#pragma once

#include "test_pattern.hpp"
#include <pl/pattern_language.hpp>
#include <pl/formatters.hpp>
#include <wolv/io/file.hpp>

namespace pl::test {

    class TestPatternFormat : public TestPattern {
    public:
        TestPatternFormat(core::Evaluator *evaluator) : TestPattern(evaluator, "Format", Mode::Succeeding) {
        }
        ~TestPatternFormat() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
                // Byte sequences decodeUtf8Codepoint() has to tell apart. A
                // format function supplies them, so the shared data file stays
                // as it is.
                //
                //   \xC3        a lead byte, then a byte that cannot follow it
                //   \x80        a continuation byte with no lead byte
                //   \xC0\xAF    a lead byte below 0xC2, which is always overlong
                //   \xE0\x80\xAF      a three byte overlong for U+002F
                //   \xF0\x80\x80\xAF  a four byte overlong for U+002F
                //   \xED\xA0\x80  a surrogate, which is not a code point
                //   \xF4\x90\x80\x80  past U+10FFFF
                //   \xF0\x9F\x98\x80  a valid four byte sequence
                //   \xF0\x9F\x98  the last one, cut short by the end of the
                //                 string. Reading its fourth byte reads past
                //                 the end.
                // Characters a formatter must escape, and text above ASCII that
                // it must not.
                fn format_tricky(str value) {
                    return "quote=\" backslash=\\ newline=\n café";
                };

                fn format_utf8(str value) {
                    return "\xC3|\x80|\xC0\xAF|\xE0\x80\xAF|\xF0\x80\x80\xAF|\xED\xA0\x80|\xF4\x90\x80\x80|\xF0\x9F\x98\x80|\xF0\x9F\x98";
                };

                struct MyStruct {
                    char s[];
                    u8 ua;
                    u16 ub;
                    u32 uc;
                    u48 ud;
                    u64 ue;
                    u128 uf;
                    s8 sa;
                    s16 sb;
                    s32 sc;
                    s48 sd;
                    s64 se;
                    // s128 sf;

                    // Last, so the offsets above do not move.
                    char tricky[1] [[format("format_tricky")]];
                    char utf8[1] [[format("format_utf8")]];
                };

                MyStruct data @ 0x0;
            )test";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            auto formatters = pl::gen::fmt::createFormatters();

            // do this to ensure new formatters will be tested (or are least considered)
            if (formatters.size() != 3) {
                throw std::runtime_error(fmt::format("Expected 3 formatters: JSON, Yaml, HTML. Got {}. If you are adding a new formatter, please add a test for it", formatters.size()));
            }

            for(auto &formatter : formatters) {
                if (formatter->getName() == "html") {
                    continue; // disable test for html formatter because there is a lot of metadata information, which may often change without indicating a problem
                }

                auto actualResultBytes = formatter->format(*this->m_runtime);
                std::string actualResult(actualResultBytes.begin(), actualResultBytes.end());

                std::string inputFilename = "./files/export/" + formatter->getName() + "." + formatter->getFileExtension();
                wolv::io::File inputFile(inputFilename, wolv::io::File::Mode::Read);
                if (!inputFile.isValid()) {
                    throw std::runtime_error(fmt::format("Could not open file {}", inputFilename));
                }
                std::string expectedResult = inputFile.readString();

                if (formatter->getName() == "json" || formatter->getName() == "yaml") {
                    // trim strings
                    actualResult = wolv::util::trim(actualResult);
                    expectedResult = wolv::util::trim(expectedResult);
                }

                if (actualResult != expectedResult) {
                    throw std::runtime_error(fmt::format("Formatter {} returned unexpected result.\nExpected: {}\nActual: {}", formatter->getName(), expectedResult, actualResult));
                }
            }

            return true;
        }

    };

}