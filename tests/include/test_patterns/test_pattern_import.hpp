#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternImport : public TestPattern {
    public:
        TestPatternImport(core::Evaluator *evaluator) : TestPattern(evaluator, "Import") {
        }
        ~TestPatternImport() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                import IA as A;
                import * from ImportedSection as ImportedSection;

                // once include tests
                import IC as C; // should do nothing, IC as C was transitively imported from A
                import IC; // should work as expected (import IC without any alias) [ c ]
                import IC; // should do nothing, IC was already imported
                import IC as C2; // should work as expected (import IC with alias C2) [ C2::c ]

                auto data = builtin::std::mem::create_section("data");
                builtin::std::mem::copy_value_to_section("AB", data, 0);

                ImportedSection imported @ 0 in data;
                std::assert(imported == 'A', "Imported type was not placed in the target section");

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
