#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternInVariableDefault : public TestPattern {
    public:
        TestPatternInVariableDefault(core::Evaluator *evaluator) : TestPattern(evaluator, "InVariableDefault") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 base = 40;
                u32 value in = base + 2;
                std::assert(value == 42, "Input variable default was not used");
            )";
        }
    };

    class TestPatternInVariableOverride : public TestPattern {
    public:
        TestPatternInVariableOverride(core::Evaluator *evaluator) : TestPattern(evaluator, "InVariableOverride") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 value in = 1 / 0;
                std::assert(value == 123, "Input variable did not override its default");
            )";
        }

        [[nodiscard]] std::map<std::string, core::Token::Literal> getInVariables() const override {
            return { { "value", u128(123) } };
        }
    };

}
