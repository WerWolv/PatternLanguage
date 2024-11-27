#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternExample : public TestPattern {
    public:
        TestPatternExample(core::Evaluator *evaluator) : TestPattern(evaluator, "") {
        }
        ~TestPatternExample() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(

            )";
        }
    };

}