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
                struct FirstFormatOnlyTest {
                    s32 x [[format("format_field1")]];
                } [[format("format_only_test")]];

                struct FormatOnlyTest {
                    s32 x [[format("format_field1")]];
                    FirstFormatOnlyTest y [[format("format_field2")]];
                } [[format("format_only_test")]];

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

                fn format_only_test(auto value) {
                    std::assert(value.x == 1196314761, "Accessing field in format in FormatOnlyTest");
                    return "Hello";
                };

                fn format_test(FormatTransformTest value) {
                    std::assert(value.x == 1196314761, "Accessing field in format in FormatTransformTest");
                    return "Hello World";
                };

                fn transform_test(FormatTransformTest value) {
                    std::assert(value.x == 1196314761, "Accessing field in transform");
                    return 1337;
                };

                fn format_field1(s32 value) {
                    return builtin::std::format("Value is {}", value);
                };
                fn format_field2(FirstFormatOnlyTest value) {
                    return builtin::std::format("Pattern value is {}", value.x);
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

                FormatOnlyTest formatOnlyTest @ 0x00;
                FormatTransformTest formatTransformTest @ 0x00;
                SealedTest sealedTest @ 0x10;
                HiddenTest hiddenTest @ 0x20;
                ColorTest colorTest @ 0x30;
                NoUniqueAddressTest noUniqueAddressTest @ 0x40;
                FixedSizeTest1 fixedSizeTest1 @ 0x50;
                FixedSizeTest2 fixedSizeTest2 @ 0x60;

                // std::format does not respect [[format]] for non-patterns values
                std::assert(builtin::std::format("{}", formatOnlyTest.x) == "1196314761", "Format attribute should not apply to non-pattern field");
                std::assert(builtin::std::format("{}", formatOnlyTest.y) == "Pattern value is 169478669", "Format attribute on field not working on pattern value");
                std::assert(builtin::std::format("{}", formatOnlyTest) == "Hello", "Format attribute not working");

                std::assert(formatTransformTest == 1337, "Transform attribute not working 1");
                std::assert(builtin::std::format("{}", formatTransformTest) == "1337", "Transform attribute not working 2");
                std::assert(sizeof(noUniqueAddressTest) == sizeof(u32), "No Unique Address attribute not working");
                std::assert(sizeof(fixedSizeTest1) == 4, "Fixed size attribute not working");
            )test";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            for (const auto &pattern : patterns) {
                auto varName = pattern->getVariableName();

                if (varName == "formatOnlyTest") {
                    if (pattern->getFormattedValue() != "Hello")
                        return false;
                    if (pattern->toString() != "Hello")
                        return false;

                    if (auto *iterable = dynamic_cast<ptrn::IIterable *>(pattern.get()); iterable != nullptr) {
                        for (const auto &entry : iterable->getEntries()) {
                            const auto entryName = entry->getVariableName();
                            if (entryName == "x") {
                                if (entry->getFormattedValue() != "Value is 1196314761")
                                    return false;
                                if (entry->toString() != "Value is 1196314761")
                                    return false;
                            } else if (entryName == "y") {
                                if (entry->getFormattedValue() != "Pattern value is 169478669")
                                    return false;
                                if (entry->toString() != "Pattern value is 169478669")
                                    return false;
                            }
                        }
                    }
                } else if (varName == "formatTransformTest") {
                    if (pattern->getFormattedValue() != "Hello World")
                        return false;
                    if (pattern->toString() != "Hello World")
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
