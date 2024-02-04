#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternImport : public TestPattern {
    public:
        TestPatternImport() : TestPattern("Import") {
        }
        ~TestPatternImport() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                import IA as A;
                // once include tests
                import IC as C; // should do nothing, IC as C was transitively imported from A
                import IC; // should work as expected (import IC without any alias) [ c ]
                import IC; // should do nothing, IC was already imported
                import IC as C2; // should work as expected (import IC with alias C2) [ C2::c ]

                fn main() {
                    A::a();
                    B::b();
                    C::c();
                    c();
                    C2::c();
                };
            )";
        }
    };

}