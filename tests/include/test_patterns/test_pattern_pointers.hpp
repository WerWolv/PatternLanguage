#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_pointer.hpp>

namespace pl::test {

    class TestPatternPointers : public TestPattern {
    public:
        TestPatternPointers() : TestPattern("Pointers") {
            // placementPointer
            {
                auto placementPointer = create<PatternPointer>("", "placementPointer", 0x0C, sizeof(u8));
                placementPointer->setPointedAtAddress(0x49);
                auto pointerType = create<PatternUnsigned>("u8", "", 0x0C, sizeof(u8));
                placementPointer->setPointerTypePattern(std::move(pointerType));

                auto pointedTo = create<PatternUnsigned>("u32", "", 0, sizeof(u32));
                placementPointer->setPointedAtPattern(std::move(pointedTo));
                addPattern(std::move(placementPointer));
            }

            // pointerToArray
            {
                auto placementPointer = create<PatternPointer>("", "pointerToArray", 0x0D, sizeof(u8));
                placementPointer->setPointedAtAddress(0x48);
                auto pointerType = create<PatternUnsigned>("u8", "", 0x0D, sizeof(u8));
                placementPointer->setPointerTypePattern(std::move(pointerType));

                auto pointedTo = create<PatternArrayStatic>("u32", "", 0, sizeof(u32[10]));
                auto arrayTemplate = create<PatternUnsigned>("u32", "", 0, sizeof(u32));
                pointedTo->setEntries(std::move(arrayTemplate), 10);
                placementPointer->setPointedAtPattern(std::move(pointedTo));
                addPattern(std::move(placementPointer));
            }

            // pointerRelativeSigned
            {
                auto placementPointer = create<PatternPointer>("", "pointerRelativeSigned", 0x1D, sizeof(u8));
                placementPointer->setPointedAtAddress(i8(0xE6));
                placementPointer->rebase(0x1D);
                auto pointerType = create<PatternSigned>("s8", "", 0x1D, sizeof(u8));
                placementPointer->setPointerTypePattern(std::move(pointerType));

                auto pointedTo = create<PatternUnsigned>("u32", "", 0, sizeof(u32));
                placementPointer->setPointedAtPattern(std::move(pointedTo));
                addPattern(std::move(placementPointer));
            }
        }
        ~TestPatternPointers() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 *placementPointer : u8 @ 0x0C;
                u32 *pointerToArray[10] : u8 @ $;

                fn Rel(u128) { return 0x1D; };
                u32 *pointerRelativeSigned : s8 @ 0x1D [[pointer_base("Rel")]];
            )";
        }
    };

}
