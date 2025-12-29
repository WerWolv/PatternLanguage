#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternUsing : public TestPattern {
    public:
        TestPatternUsing(core::Evaluator *evaluator) : TestPattern(evaluator, "Using") {
        }
        ~TestPatternUsing() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                using US<T> = T;
                US<u32> u;
                US<u32> v = 64;
                u = 64;
                std::assert(u == 64 && v == 64, "u,v should be 64");

                using UST<T> = US<T>;
                UST<u32> ust;
                UST<u32> vst = 16;
                ust = 16;
                std::assert(ust == 16 && vst == 16, "ust,vst should be 16");

                struct USS<T> {
                    US<T> us = 16;
                };
                USS<u32> uss;
                std::assert(uss.us == 16, "us should be 16");

                USS<US<u32>> ussus;
                std::assert(ussus.us == 16, "ussus should be 16");

                US<u8> us2[2];
                US<u8> us3[2] @ 0;
                std::assert(us3[0] == 137, "us3[0] should be 137");

                namespace A {
                    using US<T> = T;
                }
                A::US<u8> us4;
                A::US<u8> us5 @ 0;
                us4 = us5;
                std::assert(us5 == 137 && us4 == 137, "us4, us5 should be 137");
            )";
        }
    };

}