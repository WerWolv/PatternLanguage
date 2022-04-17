#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternExample : public TestPattern {
    public:
        TestPatternExample() : TestPattern("") {
        }
        ~TestPatternExample() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(

            )";
        }
    };

}