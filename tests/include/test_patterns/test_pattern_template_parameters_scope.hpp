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

                fn populate_exported_local_array(ref u32 output, ref u8 values) {
                    output[0] = values[0];
                    output[1] = values[1];
                };

                struct LocalExport {
                    u8 values[2];
                    u32 image[2] = { 0 } [[export]];
                    populate_exported_local_array(image, values);
                };

                LocalExport localExport @ 0;
                std::assert(localExport.image[0] == localExport.values[0], "localExport.image[0] should be populated");
                std::assert(localExport.image[1] == localExport.values[1], "localExport.image[1] should be populated");

                struct IndexedCopyTest {
                    u32 a;
                    u32 b;
                };

                IndexedCopyTest indexedCopyTests[2];
                indexedCopyTests[0].a = 1;
                indexedCopyTests[0].b = 2;
                indexedCopyTests[1].a = 3;
                indexedCopyTests[1].b = 4;

                u32 indexedCopyIndex = 0;

                struct IndexedCopyEntry {
                    IndexedCopyTest t = indexedCopyTests[indexedCopyIndex];
                    indexedCopyIndex += 1;

                    std::assert(t.a == indexedCopyIndex * 2 - 1, "Indexed local struct copy field a invalid");
                    std::assert(t.b == indexedCopyIndex * 2, "Indexed local struct copy field b invalid");
                };

                struct IndexedCopyHolder {
                    IndexedCopyEntry entries[2];
                };

                IndexedCopyHolder indexedCopyHolder @ 0x0;

                str localStringNames[3];
                localStringNames[0] = "str number 0";
                localStringNames[1] = "str number 1";
                localStringNames[2] = "str number 2";
                std::assert(localStringNames[0] == "str number 0", "localStringNames[0] should not overlap");
                std::assert(localStringNames[1] == "str number 1", "localStringNames[1] should not overlap");
                std::assert(localStringNames[2] == "str number 2", "localStringNames[2] should not overlap");

                struct StringCopyTest {
                    u8 x;
                    str name;
                };

                StringCopyTest stringCopyTests[3];
                stringCopyTests[0].x = 0;
                stringCopyTests[1].x = 1;
                stringCopyTests[2].x = 2;

                u32 stringCopyIndex = 0;
                fn put_string_copy_name() {
                    str value = builtin::std::format("str number {}", stringCopyIndex);
                    stringCopyTests[stringCopyIndex].name = value;
                    stringCopyIndex += 1;
                };

                for (u32 stringCopyLoop = 0, stringCopyLoop < 3, stringCopyLoop += 1)
                    put_string_copy_name();

                std::assert(stringCopyTests[0].name == "str number 0", "stringCopyTests[0].name should not overlap");
                std::assert(stringCopyTests[1].name == "str number 1", "stringCopyTests[1].name should not overlap");
                std::assert(stringCopyTests[2].name == "str number 2", "stringCopyTests[2].name should not overlap");

                u32 stringCopyOutputIndex = 0;
                struct StringCopyOutput {
                    str name = stringCopyTests[stringCopyOutputIndex].name [[export]];
                    std::assert(name == builtin::std::format("str number {}", stringCopyOutputIndex), "Exported local string copy invalid");
                    stringCopyOutputIndex += 1;
                };

                StringCopyOutput stringCopyOutput[3] @ 0x0;
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            for (const auto &pattern : patterns) {
                if (pattern->getVariableName() != "localExport")
                    continue;

                auto iterable = dynamic_cast<ptrn::IIterable *>(pattern.get());
                if (iterable == nullptr)
                    return false;

                std::shared_ptr<ptrn::Pattern> image;
                iterable->forEachEntry(0, iterable->getEntryCount(), [&](u64, const auto &entry) {
                    if (entry->getVariableName() == "image")
                        image = entry;
                });

                if (image == nullptr)
                    return false;

                auto imageIterable = dynamic_cast<ptrn::IIterable *>(image.get());
                if (imageIterable == nullptr)
                    return false;

                u64 entryCount = 0;
                imageIterable->forEachEntry(0, imageIterable->getEntryCount(), [&](u64, const auto &) {
                    entryCount += 1;
                });

                return entryCount == 2;
            }

            return false;
        }
    };

}
