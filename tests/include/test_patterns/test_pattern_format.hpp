#pragma once

#include "test_pattern.hpp"
#include <pl/pattern_language.hpp>
#include <pl/formatters.hpp>
#include <wolv/io/file.hpp>

namespace pl::test {

    class TestPatternFormat : public TestPattern {
    public:
        TestPatternFormat() : TestPattern("Format", Mode::Succeeding) {
        }
        ~TestPatternFormat() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
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

                std::string inputFilename = "../tests/files/export/"+formatter->getName()+"."+formatter->getFileExtension();
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