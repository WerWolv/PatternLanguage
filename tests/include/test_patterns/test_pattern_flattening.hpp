#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternFlattening : public TestPattern {
    public:
        TestPatternFlattening(core::Evaluator *evaluator) : TestPattern(evaluator, "Flattening") {
        }
        ~TestPatternFlattening() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Outer {
                    u8 bytes[16];
                } [[sealed]];

                struct Inner {
                    u8 bytes[14];
                } [[sealed]];

                Outer outer @ 0;
                Inner inner @ 0;

                u8 before1 @ 1;
                u8 before2 @ 2;
                u8 before3 @ 3;
                u8 before4 @ 4;
                u8 before5 @ 5;
                u8 before6 @ 6;
                u8 before7 @ 7;
                u8 before8 @ 8;
                u8 before9 @ 9;
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            wolv::util::unused(patterns);

            bool foundOuter = false;
            bool foundInner = false;

            for (const auto *pattern : m_runtime->getPatternsAtAddress(12)) {
                foundOuter |= pattern->getVariableName() == "outer";
                foundInner |= pattern->getVariableName() == "inner";
            }

            return foundOuter && foundInner;
        }
    };

}
