#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternAllowedRedefinitions : public TestPattern {
    public:
        TestPatternAllowedRedefinitions(core::Evaluator *evaluator) : TestPattern(evaluator, "AllowedRedefinitions") {
        }
        ~TestPatternAllowedRedefinitions() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct S {
                    u8;
                    padding[3];
                    u8;
                    padding[3];
                };
            )";
        }
    };

}