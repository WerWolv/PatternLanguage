#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternAttributes : public TestPattern {
    public:
        TestPatternAttributes() : TestPattern("Attributes") {
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

                FormatTransformTest formatTransformTest @ 0x00;
                SealedTest sealedTest @ 0x10;
                HiddenTest hiddenTest @ 0x20;
                ColorTest colorTest @ 0x30;
                NoUniqueAddressTest noUniqueAddressTest @ 0x40;

                std::assert(formatTransformTest == 1337, "Transform attribute not working");
                std::assert(sizeof(noUniqueAddressTest) == sizeof(u32), "No Unique Address attribute not working");
            )test";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            for (const auto &pattern : patterns) {
                auto &varName = pattern->getVariableName();

                if (varName == "formatTransformTest") {
                    if (pattern->getFormattedValue() != "Hello World")
                        return false;
                } else if (varName == "sealedTest") {
                    if (!pattern->isSealed())
                        return false;
                } else if (varName == "hiddenTest") {
                    if (!pattern->isHidden())
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