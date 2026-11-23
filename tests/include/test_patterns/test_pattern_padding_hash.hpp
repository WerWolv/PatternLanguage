#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternPaddingHash : public TestPattern {
    public:
        TestPatternPaddingHash(core::Evaluator *evaluator) : TestPattern(evaluator, "PaddingHash") {
        }
        ~TestPatternPaddingHash() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                import std.hash;

                struct WithPadding {
                    u8 a;
                    padding[2];
                    u8 b;
                };

                struct WithoutPadding {
                    u8 a;
                    u8 p0;
                    u8 p1;
                    u8 b;
                };

                WithPadding withPadding @ 0x00;
                WithoutPadding withoutPadding @ 0x00;

                std::assert(
                    std::hash::crc32(withPadding, 0xFFFFFFFF, 0x04C11DB7, 0xFFFFFFFF, true, true) ==
                    std::hash::crc32(withoutPadding, 0xFFFFFFFF, 0x04C11DB7, 0xFFFFFFFF, true, true),
                    "crc32 must include padding bytes"
                );
            )";
        }
    };

}
