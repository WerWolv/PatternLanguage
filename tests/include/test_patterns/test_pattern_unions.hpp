#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_union.hpp>

namespace pl::test {

    class TestPatternUnions : public TestPattern {
    public:
        TestPatternUnions(core::Evaluator *evaluator) : TestPattern(evaluator, "Unions") {
            auto testUnion = create<PatternUnion>("TestUnion", "testUnion", 0x200, sizeof(u128), 0);

            auto array = create<PatternArrayStatic>("s32", "array", 0x200, sizeof(i32[2]), 0);
            array->setEntries(create<PatternSigned>("s32", "", 0x200, sizeof(i32), 0), 2);
            auto variable = create<PatternUnsigned>("u128", "variable", 0x200, sizeof(u128), 0);

            std::vector<std::shared_ptr<Pattern>> unionMembers;
            {
                unionMembers.push_back(std::move(array));
                unionMembers.push_back(std::move(variable));
            }

            testUnion->setEntries(std::move(unionMembers));

            addPattern(std::move(testUnion));
        }
        ~TestPatternUnions() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                union TestUnion {
                    s32 array[2];
                    if ( true ) { u128 variable; }
                };

                TestUnion testUnion @ 0x200;
            )";
        }
    };

}