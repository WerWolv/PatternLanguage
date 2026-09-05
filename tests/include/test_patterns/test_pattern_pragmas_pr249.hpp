#pragma once

#include "test_pattern.hpp"
#include <pl/pattern_language.hpp>

namespace pl::test {

    class TestPatternPragmasIssue249 : public TestPattern {
    public:
        TestPatternPragmasIssue249(core::Evaluator *evaluator) : TestPattern(evaluator, "PragmasIssue249") {
        }
        ~TestPatternPragmasIssue249() override = default;

        void setup() override {
            // All pragmas should be processed the same way, but we test multiple ones to be sure
            m_runtime->addPragma("once", [](PatternLanguage&, const std::string &value) {
                return true;
            });
            m_runtime->addPragma("author", [](PatternLanguage&, const std::string &value) {
                return value == "authorValue";
            });
            m_runtime->addPragma("description", [](PatternLanguage&, const std::string &value) {
                return value == "descValue";
            });
            m_runtime->addPragma("somePragma", [](PatternLanguage&, const std::string &value) {
                return value == "someValue";
            });
            // Also test a pragma which isn't defined in the code, ans whose callback should not be called
            m_runtime->addPragma("unknownPragma", [](PatternLanguage&, const std::string &value) {
                return false;
            });
        };

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
                #pragma once
                #pragma author authorValue
                #pragma description descValue
                #pragma somePragma someValue

                u8 test = 0;
            )test";
        }

    };

}