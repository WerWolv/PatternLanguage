#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_array_static.hpp>

namespace pl::test {

    class TestPatternStructInheritance : public TestPattern {
    public:
        TestPatternStructInheritance() : TestPattern("StructInheritance") {
            auto childStruct = create<PatternStruct>("Child", "test", 0x0, sizeof(u32) * 2);

            auto inheritedVariable = create<PatternUnsigned>("u32", "inherited", 0x0, sizeof(u32));
            auto ownVariable       = create<PatternUnsigned>("u32", "own", 0x0 + sizeof(u32), sizeof(u32));

            std::vector<std::shared_ptr<Pattern>> structMembers;
            {
                structMembers.push_back(std::move(inheritedVariable));
                structMembers.push_back(std::move(ownVariable));
            }
            childStruct->setMembers(std::move(structMembers));

            addPattern(std::move(childStruct));
        }
        ~TestPatternStructInheritance() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Parent<T> {
                    T inherited;
                };

                struct Child<T> : Parent<T> {
                    T own;
                };

                Child<u32> test @ 0;

                std::assert(test.inherited == 0x474E5089, "Inherited field invalid");
                std::assert(test.own == 0x0A1A0A0D, "Own field invalid");
            )";
        }
    };

}