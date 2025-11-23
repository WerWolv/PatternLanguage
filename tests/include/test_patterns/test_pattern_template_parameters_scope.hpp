#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternTemplateParametersScope : public TestPattern {
    public:
        TestPatternTemplateParametersScope(core::Evaluator *evaluator) : TestPattern(evaluator, "TemplateParametersScope") {
        }
        ~TestPatternTemplateParametersScope() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Y {
                    try {
                        u8 a = x;
                    } catch {
                        u8 a = 2;
                    }
                };
                struct T<auto x> {
                    u8 value = x;
                    Y y;
                };
                T<4> t @ 0;
                std::assert(t.y.a == 2, "t.value should be 2");

                struct U<V> {
                    V v;
                };
                struct S {
                    u8 j = 16;
                    U<T<j>> t;
                };
                S s @ 0 ;
                std::assert(s.t.v.value == 16, "s.t.v.value should be 16");

                struct W {
                    T<parent.j> t;
                };
                struct R {
                    u8 j = 32;
                    W w;
                };
                R r @ 0;
                std::assert(r.w.t.value == 32, "r.w.t.value should be 32");

                struct Z<T, auto v> {
                    if(v > 0) {
                        Z<Z<T, v - 1>, v-1> q;
                    }
                    u32 value = v;
                };
                Z<u32, 5> a @ 0;

                std::assert(a.q.q.q.q.q.value == 0, "a.q.q.q.q.q.value should be 0");

                u32 b = 64;
                using Alias = U<T<b>>;
                Alias alias @ 0;
                std::assert(alias.v.value == 64, "alias.v.value should be 32");

                using C<auto d> = U<T<d + b>>;
                C<16> c @ 0;

                std::assert(c.v.value == 80, "c.v.value should be 80");

                struct E {
                    u32 b = 128;
                    C<b> c;
                };
                E e @ 0;
                std::assert(e.c.v.value == 192, "e.c.v.value should be 192");

                using G<auto b, c>;
                struct H{
                    G<66, u32> d;
                };
                H h @0;
                struct G<auto b, c> {
                    c cc = b;
                };
                std::assert(h.d.cc == 66, "f.x should be 66");

                using I<type, auto value>;
                struct J<auto value, type> {
                    if(value > 0) {
                        I<type, value - 1 > i;
                    }
                    u32 j = value [[export]];
                };
                struct I<type, auto value> {
                    J<value, type> j;
                    u32 i = value [[export]];
                };
                I<u32, 3> i @ 0;
                std::assert(i.j.i.j.i.j.i.j.j == 0, "i.j.i.j.i.j.i.j.j should be 0");

                struct K<auto v> {
                    u32 e = v;
                };
                using P<auto x> = K<x + b>;
                struct Q {
                    u32 b = 1;
                    P<1> q;
                };
                Q q @ 0;
                std::assert(q.q.e == 65, "q.q.e should be 65");

                using Fp<auto x>;  
                struct FFp{
                    Fp<1> fp;
                };
                FFp ffp @ 0;
                using Fp<auto x> = P<x + b>;
                std::assert(ffp.fp.e == 129, "fp.e should be 129");
            )";
        }
    };
}