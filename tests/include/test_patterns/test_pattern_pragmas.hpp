#pragma once

#include "test_pattern.hpp"
#include <pl/pattern_language.hpp>

namespace pl::test {

    class TestPatternPragmas : public TestPattern {
    public:
        TestPatternPragmas() : TestPattern("Pragmas") {
        }
        ~TestPatternPragmas() override = default;

        void setupRuntime(pl::PatternLanguage &runtime) {
            // All pragmas should be processed the same way, but we test multiple ones to be sure
            runtime.addPragma("author", [](PatternLanguage&, const std::string &value) {
                return value == "authorValue";
            });
            runtime.addPragma("description", [](PatternLanguage&, const std::string &value) {
                return value == "descValue";
            });
            runtime.addPragma("somePragma", [](PatternLanguage&, const std::string &value) {
                return value == "someValue";
            });
            // Also test a pragma which isn't defined in the code, ans whose callback should not be called
            runtime.addPragma("unknownPragma", [](PatternLanguage&, const std::string &value) {
                return false;
            });
        };

        [[nodiscard]] std::string getSourceCode() const override {
            return R"test(
                #pragma author authorValue
                #pragma description descValue
                #pragma somePragma someValue

                u8 test = 0;
            )test";
        }

    };

}