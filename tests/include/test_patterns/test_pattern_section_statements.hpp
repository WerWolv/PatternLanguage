#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternSectionStatements : public TestPattern {
    public:
        TestPatternSectionStatements(core::Evaluator *evaluator) : TestPattern(evaluator, "SectionStatements") {
        }
        ~TestPatternSectionStatements() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                auto section = builtin::std::mem::create_section("data");
                builtin::std::mem::copy_value_to_section("ABCDEFGHIJKL", section, 0);

                struct Inner {
                    u8 a;
                };

                struct S {
                    u8 plain;
                    match (plain) {
                        (_): u8 inMatch;
                    }
                    if (true) {
                        u8 inIf;
                    }
                    try {
                        u8 inTry;
                    } catch { }
                    try {
                        u8 failing[1] @ 0x100;
                        u8 failed = failing[1];
                    } catch {
                        u8 inCatch;
                    }
                    Inner inner;
                    match (plain) {
                        (_): Inner structInMatch;
                    }
                };

                S s @ 0x00 in section;
                std::assert(s.plain == 0x41, "Struct member did not read from section");
                std::assert(s.inMatch == 0x42, "Match arm member did not read from section");
                std::assert(s.inIf == 0x43, "If body member did not read from section");
                std::assert(s.inTry == 0x44, "Try body member did not read from section");
                std::assert(s.inCatch == 0x45, "Catch body member did not read from section");
                std::assert(s.inner.a == 0x46, "Nested struct member did not read from section");
                std::assert(s.structInMatch.a == 0x47, "Struct in match arm did not read from section");
            )";
        }
    };

}
