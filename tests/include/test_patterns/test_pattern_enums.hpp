#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_enum.hpp>

namespace pl::test {

    class TestPatternEnums : public TestPattern {
    public:
        TestPatternEnums() : TestPattern("Enums") {
            auto testEnum = create<PatternEnum>("TestEnum", "testEnum", 0x08, sizeof(u32));
            testEnum->setEnumValues({
                { u128(0x00), u128(0x00), "A" },
                { i128(0x0C), i128(0x0C), "B" },
                { u128(0x0D), u128(0x0D), "C" },
                { u128(0x0E), u128(0x0E), "D" },
                { i128(0xAA), i128(0xBB), "E" },
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