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

                fn main() {
                    A::a();
                    B::b();
                    C::c();
                };
            )";
        }
    };

}