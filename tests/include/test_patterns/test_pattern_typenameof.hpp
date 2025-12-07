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
                A q @ 0;
                B<u32, q> r;
                TypeName<u32, "u32"> a @ 0;
                TypeName<A, "A"> b @ 0;
                TypeName<B<u32, 2>, "B<u32, 2>"> c @ 0;
                TypeName<B<B<u32, P>, 2>, "B<B<u32, 16>, 2>"> d @ 0;
                TypeName<TypeName<A, "A"> , "TypeName<A, \"A\">"> e @ 0;
                TypeName<B<u32, q>, "B<u32, A{ }>"> f @ 0;
                std::assert(typenameof(B<B<u32, P>, 2>) == "B<B<u32, 16>, 2>", "type name should match");
                std::assert(typenameof(B<u32, q>) == "B<u32, A{ }>", "type name should match");

                B<u32, P> t;
                std::assert(typenameof(t) == "B<u32, 16>", "type name should match");

                struct S {
                    u32 e = 16;
                    B<u32, e> a = t;
                    str typename = typenameof(a);
                };

                S s @ 0;
                std::assert(s.typename == "B<u32, 16>", "type name should match");

                struct U {
                    u8 size;
                    u32 value[size];
                };

                std::assert(typenameof(U) == "U", "type name should match");
            )";
        }
    };

}