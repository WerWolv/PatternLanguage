#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternTypeNameOf : public TestPattern {
    public:
        TestPatternTypeNameOf(core::Evaluator *evaluator) : TestPattern(evaluator, "TypeNameOf") {
        }
        ~TestPatternTypeNameOf() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct A {

                };

                struct B<T, auto y> {

                };

                struct TypeName <type, auto typename> {
                    std::assert(typenameof(type) == typename, "type name should match");
                };

                u32 P = 16;
                TypeName<u32, "u32"> a @ 0;
                TypeName<A, "A"> b @ 0;
                TypeName<B<u32, 2>, "B<u32, 2>"> c @ 0;
                TypeName<B<B<u32, P>, 2>, "B<B<u32, 16>, 2>"> d @ 0;
                TypeName<TypeName<A, "A"> , "TypeName<A, \"A\">"> e @ 0;
                std::assert(typenameof(B<B<u32, P>, 2>) == "B<B<u32, 16>, 2>", "type name should match");
            )";
        }
    };

}