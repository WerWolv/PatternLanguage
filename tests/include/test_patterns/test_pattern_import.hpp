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

                struct CurrentSectionRead {
                    u8 first = builtin::std::mem::read_unsigned(0, 1, 0);
                    u8 firstViaSentinel = builtin::std::mem::read_unsigned(0, 1, 0, 0xFFFFFFFFFFFFFFFF);
                };

                ImportedSection imported @ 0 in data;
                std::assert(imported == 'A', "Imported type was not placed in the target section");

                CurrentSectionRead currentSectionRead @ 0 in data;
                std::assert(currentSectionRead.first == 'A', "std::mem did not use the current section by default");
                std::assert(currentSectionRead.firstViaSentinel == 'A', "std::mem did not resolve the current-section sentinel");

                fn check_std_mem_internal_section_rejection() {
                    bool rejectedPatternLocal = false;

                    try {
                        builtin::std::mem::read_unsigned(0, 1, 0, 0xFFFFFFFFFFFFFFFE);
                    } catch {
                        rejectedPatternLocal = true;
                    }

                    std::assert(rejectedPatternLocal, "std::mem allowed access to the pattern-local section");
                };

                fn main() {
                    A::a();
                    B::b();
                    C::c();
                    c();
                    C2::c();
                    check_std_mem_internal_section_rejection();
                };
            )";
        }
    };

}
