#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternAttributes : public TestPattern {
    public:
        TestPatternAttributes(core::Evaluator *evaluator) : TestPattern(evaluator, "Attributes") {
        }
        ~TestPatternAttributes() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
                struct FormatTransformTest {
                    u32 x, y, z;
                } [[format("format_test"), transform("transform_test")]];

                struct SealedTest {
                    float f;
                } [[sealed]];

                struct HiddenTest {
                    double f;
                } [[hidden]];

                struct ColorTest {
                    char s[5];
                } [[color("FF00FF")]];

                struct NoUniqueAddressTest {
                    u32 x;
                    u32 y [[no_unique_address]];
                };

                fn format_test(FormatTransformTest value) {
                    return "Hello World";
                };

                fn transform_test(FormatTransformTest value) {
                    return 1337;
                };

                struct FixedSizeTest1 {
                    u8 x;
                } [[fixed_size(4)]];

                struct FixedSizeTest2 {
                    u32 pos = $;
                    u32 elem [[fixed_size(7)]];
                    std::assert($ == pos + 7, "Fixed size variable attribute padding not working");

                    pos = $;
                    FixedSizeTest1 fixedSizeTest1;
                    std::assert($ == pos + 4, "Fixed size pattern attribute padding not working");
                };

                FormatTransformTest formatTransformTest @ 0x00;
                SealedTest sealedTest @ 0x10;
                HiddenTest hiddenTest @ 0x20;
                ColorTest colorTest @ 0x30;
                NoUniqueAddressTest noUniqueAddressTest @ 0x40;
                FixedSizeTest1 fixedSizeTest1 @ 0x50;
                FixedSizeTest2 fixedSizeTest2 @ 0x60;

                std::assert(formatTransformTest == 1337, "Transform attribute not working");
                std::assert(sizeof(noUniqueAddressTest) == sizeof(u32), "No Unique Address attribute not working");
                std::assert(sizeof(fixedSizeTest1) == 4, "Fixed size attribute not working");
            )test";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            for (const auto &pattern : patterns) {
                auto varName = pattern->getVariableName();

                if (varName == "formatTransformTest") {
                    if (pattern->getFormattedValue() != "Hello World")
                        return false;
                } else if (varName == "sealedTest") {
                    if (!pattern->isSealed())
                        return false;
                } else if (varName == "hiddenTest") {
                    if (pattern->getVisibility() != ptrn::Visibility::Hidden)
                        return false;
                } else if (varName == "colorTest") {
                    if (pattern->getColor() != 0xFF00FF)
                        return false;
                }
            }

            return true;
        }

    };

}