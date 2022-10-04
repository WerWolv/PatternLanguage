#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternSucceedingAssert : public TestPattern {
    public:
        TestPatternSucceedingAssert() : TestPattern("SucceedingAssert") {
        }
        ~TestPatternSucceedingAssert() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #define MSG "Error"

                std::assert(true, MSG);
                std::assert(100 == 100, MSG);
                std::assert(50 < 100, MSG);
                std::assert(1, MSG);

                struct Test<A, B, C> {
                    A x;
                    B y;
                    C z;
                };

                Test<u32, u16, u8> test1 @ 0x00;
                Test<float, double, u64> test2 @ 0x00;

            )";
        }
    };

}