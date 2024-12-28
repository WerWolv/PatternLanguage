#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternRValuesAssignmentInStruct : public TestPattern {
    public:
        TestPatternRValuesAssignmentInStruct(core::Evaluator *evaluator) : TestPattern(evaluator, "RValuesAssignmentInStruct") {
        }
        ~TestPatternRValuesAssignmentInStruct() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct A {
                    u32 a;
                };

                struct B {
                    A a = null;
                    u8 b [5] = {1, 2, 3, 4, 5};
                    a.a = 4;
                    b[3] = 7;
                    std::assert(a.a == 4, "RValue member assignment failed!");
                    std::assert(b[3] == 7, "RValue array assignment failed!");
                };

                B a @ 0x00;
            )";
        }
    };

}