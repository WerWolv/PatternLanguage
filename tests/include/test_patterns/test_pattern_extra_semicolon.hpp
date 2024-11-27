#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternExtraSemicolon : public TestPattern {
    public:
        TestPatternExtraSemicolon(core::Evaluator *evaluator) : TestPattern(evaluator, "ExtraSemicolon") {
        }
        ~TestPatternExtraSemicolon() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Test {
                    u32 x;;;
                    u8 y;
                    float z;;
                };;

                struct Test2 {
                    u32 x;
                    u32 y;
                };

                Test test @ 0x00;;;
                Test test2 @ 0x10;
            )";
        }
    };

}