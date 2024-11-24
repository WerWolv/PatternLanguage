#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_array_static.hpp>

namespace pl::test {

    class TestPatternPlacement : public TestPattern {
    public:
        TestPatternPlacement() : TestPattern("Placement") {
            // placementVar
            {
                addPattern(create<PatternUnsigned>("u32", "placementVar", 0x00, sizeof(u32), 0));
            }

            // placementArray
            {
                auto placementArray = create<PatternArrayStatic>("u8", "placementArray", 0x10, sizeof(u8) * 10, 0);
                placementArray->setEntries(create<PatternUnsigned>("u8", "", 0x10, sizeof(u8), 0), 10);
                addPattern(std::move(placementArray));
            }
        }
        ~TestPatternPlacement() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 placementVar @ 0x00;
                u8 placementArray[10] @ 0x10;
            )";
        }
    };

}