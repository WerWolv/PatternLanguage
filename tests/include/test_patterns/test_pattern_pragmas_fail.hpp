#pragma once

#include "test_pattern.hpp"
#include <pl/pattern_language.hpp>

namespace pl::test {

    class TestPatternPragmasFail : public TestPattern {
    public:
        TestPatternPragmasFail(core::Evaluator *evaluator) : TestPattern(evaluator, "PragmasFail", Mode::Failing) {
        }
        ~TestPatternPragmasFail() override = default;

        void setup() override {
            m_runtime->addPragma("somePragma", [](PatternLanguage&, const std::string &value) {
                return value == "invalidValue";
            });
            
        };

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
                #pragma somePragma someValue

                u8 test = 0;
            )test";
        }

    };

}