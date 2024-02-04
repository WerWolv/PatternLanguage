#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternStrings : public TestPattern {
    public:
        TestPatternStrings() : TestPattern("Strings") {
        }
        ~TestPatternStrings() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                str string = "Hello World!";
                str string2 = "Hello World!\n";
                str string3 = "\"Hello World!\"";
                str string4 = "\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D";
                str string5 = "\u1734\u2162\u2264";
            )";
        }
    };

}