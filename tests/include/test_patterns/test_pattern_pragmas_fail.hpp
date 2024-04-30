#pragma once

#include "test_pattern.hpp"
#include <pl/pattern_language.hpp>

namespace pl::test {

    class TestPatternPragmasFail : public TestPattern {
    public:
        TestPatternPragmasFail() : TestPattern("PragmasFail", Mode::Failing) {
        }
        ~TestPatternPragmasFail() override = default;

        void setupRuntime(pl::PatternLanguage &runtime) {
            // All pragmas should be processed the same way, but we test multiple ones to be sure
            runtime.addPragma("somePragma", [](PatternLanguage&, const std::string &value) {
                return value == "invalidValue";
            });
            
        };

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
                #pragma somePragma someValue

                u8 test = 0;
            )test";
        }

    };

}