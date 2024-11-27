#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_enum.hpp>

namespace pl::test {

    class TestPatternEnums : public TestPattern {
    public:
        TestPatternEnums(core::Evaluator *evaluator) : TestPattern(evaluator, "Enums") {
            auto testEnum = create<PatternEnum>("TestEnum", "testEnum", 0x08, sizeof(u32), 0);
            testEnum->setEnumValues({
                { "A", { u128(0x00), u128(0x00) } },
                { "B", { i128(0x0C), i128(0x0C) } },
                { "C", { u128(0x0D), u128(0x0D) } },
                { "D", { u128(0x0E), u128(0x0E) } },
                { "E", { i128(0xAA), i128(0xBB) } },
            });
            testEnum->setEndian(std::endian::big);

            addPattern(std::move(testEnum));
        }
        ~TestPatternEnums() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                enum TestEnum : u32 {
                    A,
                    B = 0x0C,
                    C,
                    D,
                    E = 0xAA ... 0xBB
                };

                be TestEnum testEnum @ 0x08;

                std::assert(testEnum == TestEnum::C, "Invalid enum value");
            )";
        }
    };

}