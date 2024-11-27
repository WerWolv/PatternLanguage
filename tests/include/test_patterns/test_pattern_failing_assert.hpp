#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternFailingAssert : public TestPattern {
    public:
        TestPatternFailingAssert(core::Evaluator *evaluator) : TestPattern(evaluator, "FailingAssert", Mode::Failing) {
        }
        ~TestPatternFailingAssert() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #define MSG "Error"

                std::assert(false, MSG);
            )";
        }
    };

}