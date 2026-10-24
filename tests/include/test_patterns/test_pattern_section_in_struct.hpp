#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternSectionInStruct : public TestPattern {
    public:
        TestPatternSectionInStruct(core::Evaluator *evaluator) : TestPattern(evaluator, "SectionInStruct") {
        }
        ~TestPatternSectionInStruct() override = default;

        // Declaring the struct creates further patterns in the same scope that holds `foo`, which
        // grows and relocates that scope's pattern vector. The declaration must look the variable
        // up again afterwards instead of holding on to the vector's previous storage.
        //
        // Keep `foo` unplaced: this exercises the pattern-local declaration path. Writing
        // `Foo foo @ 0x00` takes the placed-declaration path instead and no longer covers the
        // defect. Pattern-local declarations deliberately produce no entries in getPatterns(),
        // so there is no resulting value to assert on here.
        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Foo {
                    u128 sec = builtin::std::mem::create_section("sec");
                    u8 array[12] @ 0x00 in sec;
                };

                Foo foo;
            )";
        }

        // The relocation only leaves unusable storage behind on some runs, so repeat the
        // evaluation often enough that a single clean run cannot hide a regression.
        [[nodiscard]] size_t repeatTimes() const override {
            return 32;
        }

        // Structural guard rather than a value check: should the source above ever be changed to a
        // placed declaration, patterns would appear here and this fails, flagging that the test no
        // longer covers the path it was written for.
        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            return patterns.empty();
        }
    };

}
