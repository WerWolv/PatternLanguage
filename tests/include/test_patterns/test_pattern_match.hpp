#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_unsigned.hpp>

namespace pl::test {

    class TestPatternMatching : public TestPattern {
    public:
        TestPatternMatching() : TestPattern("Matching") {
            auto testStruct = create<PatternStruct>("a", "b", 0x100, 3);

            std::vector<std::shared_ptr<Pattern>> members;
            {
                members.push_back(create<PatternUnsigned>("u8", "c", 0x100, sizeof(u8)));
                members.push_back(create<PatternUnsigned>("u8", "d", 0x101, sizeof(u8)));
                members.push_back(create<PatternUnsigned>("u8", "l", 0x102, sizeof(u8)));
            }

            testStruct->setMembers(std::move(members));

            addPattern(std::move(testStruct));
        }
        ~TestPatternMatching() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct a {
                    u8 c;
                    u8 d;
                    match(c, d) {
                        ((0x78 - 0x10) | 5 | 3, _): u8 h;
                        (0x77 | 0x80 | 0x87, _): u8 l;
                        (_, _): u8 k;
                    }
                };

                a b @ 0x100;
            )";
        }
    };

}