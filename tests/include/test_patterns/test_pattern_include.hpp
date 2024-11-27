#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternInclude : public TestPattern {
    public:
        TestPatternInclude(core::Evaluator *evaluator) : TestPattern(evaluator, "Include") {
        }
        ~TestPatternInclude() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #include <A>

                fn main() {
                    a();
                    b();
                    c();
                };
            )";
        }
    };

}