#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternDocComments : public TestPattern {
    public:
        TestPatternDocComments() : TestPattern("DocComments") {

            auto variablePattern = create<PatternUnsigned>("u8", "documentedVariable", 0x00, sizeof(u8), 0);

            auto structPattern = create<PatternStruct>("DocumentedStruct", "documentedStruct", 0x00, sizeof(u8) + sizeof(u8), 0);

            std::vector<std::shared_ptr<Pattern>> structMembers;
            {
                structMembers.push_back(create<PatternUnsigned>("u8", "documentedVariable", 0x00, sizeof(u8), 0));
                structMembers.push_back(create<PatternUnsigned>("u8", "documentedVariable2", 0x01, sizeof(u8), 0));
            }

            structPattern->setEntries(std::move(structMembers));

        }
        ~TestPatternDocComments() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                /// this is a doc comment
                u8 documentedVariable = 3;
                /**
                 * This is a doc comment, but with extra flare
                 */
                struct DocumentedStruct {
                    /**
                     * This is a doc comment, but with extra flare
                     */
                    u8 documentedVariable = 3;
                    /**
                     * This is a doc comment, but with extra flare
                     */
                    u8 documentedVariable2 = 3;
                };

                /*!
                 * This is a global doc comment
                 */
            )";
        }
    };

}