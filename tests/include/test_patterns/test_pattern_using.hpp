#pragma once

#include "test_pattern.hpp"
#include <pl/core/parser.hpp>
#include <pl/cli/subcommands/docs.hpp>

namespace pl::test {

    class TestPatternUsing : public TestPattern {
    public:
        TestPatternUsing(core::Evaluator *evaluator) : TestPattern(evaluator, "Using") {
        }
        ~TestPatternUsing() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                using US<T> = T;
                US<u32> u;
                US<u32> v = 64;
                u = 64;
                std::assert(u == 64 && v == 64, "u,v should be 64");

                using UST<T> = US<T>;
                UST<u32> ust;
                UST<u32> vst = 16;
                ust = 16;
                std::assert(ust == 16 && vst == 16, "ust,vst should be 16");

                struct USS<T> {
                    US<T> us = 16;
                };
                USS<u32> uss;
                std::assert(uss.us == 16, "us should be 16");

                USS<US<u32>> ussus;
                std::assert(ussus.us == 16, "ussus should be 16");

                US<u8> us2[2];
                US<u8> us3[2] @ 0;
                std::assert(us3[0] == 137, "us3[0] should be 137");

                namespace A {
                    using US<T> = T;
                }
                A::US<u8> us4;
                A::US<u8> us5 @ 0;
                us4 = us5;
                std::assert(us5 == 137 && us4 == 137, "us4, us5 should be 137");
            )";
        }
    };

    class TestPatternUsingDocs : public TestPattern {
    public:
        TestPatternUsingDocs(core::Evaluator *evaluator) : TestPattern(evaluator, "UsingDocs") {
        }
        ~TestPatternUsingDocs() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                // Aliases
                using Alias<T> = T;
                using AliasChain<T> = Alias<T>;
                using MultiAlias<R, G, B, N> = Alias<R>;
                using AliasWrap<T> = Alias<Alias<T>>;

                // Base types for expression tests
                using Base<auto N> = u8;
                using Bin<auto A, auto B> = u8;
                using Triplet<auto A, auto B, auto C> = u8;

                // Simple expressions: arithmetic on template params
                using ExprAdd<auto N> = Base<N + 1>;
                using ExprAddVar<auto N> = Base<N + 1 + 5>;

                // Comparison expressions (need parens to disambiguate from template syntax)
                using ExprLt<auto N> = Base<(N < 5)>;
                using ExprGt<auto N> = Base<(N > 5)>;

                // Function call in expression
                using ExprFunc<auto N> = Base<K(N < 5)>;

                // Ternary expression
                using ExprTernary<auto N> = Base<(0 ? 1 : 2)>;

                // Evaluated sizeof (parser folds sizeof(builtin) to a literal before docs see it)
                using ExprSizeofLit<auto N> = Base<sizeof(u32)>;

                // Cast expression
                using ExprCast<auto N> = Base<(0 as u8)>;

                // Type operators
                using ExprSizeofType<T, auto N> = Base<sizeof(T)>;
                using ExprSizeofDollar<T, auto N> = Base<sizeof($)>;

                // Nested: sizeof of type application with expression
                using NestedBase<T, auto N> = Base<(sizeof(T) + N)>;
                using NestedDeep<T, auto N> = Base<(sizeof(NestedBase<T, N - 1>) + N)>;

                // Multi-param auto
                using MultiParam<auto A, auto B, auto C, auto D> = Base<(A + B + C + D)>;

                // Multi-type (type + type + auto)
                using MultiType<T, U, auto N> = Base<(sizeof(T) + sizeof(U) + N)>;

                // Ternary with sizeof
                using TernarySizeof<T, auto N> = Base<(sizeof(T) < N ? sizeof(T) : N)>;
                using TernaryNested<T, auto N> = Base<(sizeof(T) < N ? (sizeof(T) > 0 ? sizeof(T) : 1) : N)>;

                // Multi-type ternary
                using TernaryMultiType<T, U, auto N> = Base<(sizeof(T) + sizeof(U) + N < 100 ? sizeof(T) + sizeof(U) + N : 100)>;

                // Binary ops
                using BinaryAdd<auto A, auto B> = Base<(A + B)>;

                // Multi-op chains
                using ChainOps<auto N> = Base<(N + 1) * (2 + 3)>;

                // Ternary chains
                using ChainTernary<auto N> = Base<(N == 0 ? 1 : (N == 1 ? 2 : 3))>;

                // sizeof + auto
                using SizeofAuto<T, auto N> = Base<(sizeof(T) + N)>;

                // Two-type with auto math
                using TwoTypeAuto<T, U, auto N> = Base<(sizeof(T) + N) * (sizeof(U) + N)>;

                // Complex multi-param expression
                using ComplexExpr<auto A, auto B> = Base<(A + B) * (A - B)>;

                // Ternary with auto comparison
                using TernaryAuto<auto A, auto B> = Base<(A < B ? A : (B < A ? B : A + B))>;

                // Deep ternary
                using DeepTernary<T, auto N> = Base<(sizeof(T) < N ? (sizeof(T) > 0 ? sizeof(T) : 1) : (N > 0 ? N : 1))>;

                // sizeof of type applications
                using SizeofTypeApp<T, auto N> = Base<sizeof(ExprAdd<N + 1>)>;
                using SizeofTypeAppExpr<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) + N) * (sizeof(ExprAdd<N + 2>) + N)>;

                // Two-type sizeof comparison
                using TwoTypeCompare<T, U, auto N> = Base<(sizeof(ExprAdd<N>) < sizeof(ExprAdd<N + 1>) ? sizeof(ExprAdd<N>) : sizeof(ExprAdd<N + 1>))>;

                // Cast + math
                using CastMath<T, auto N> = Base<(N + 1 as u8)>;

                // Cast on sizeof
                using CastSizeof<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8)>;
                using CastSizeofMulti<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8) + (sizeof(ExprAdd<N + 2>) as u16)>;

                // sizeof of 2-param type app with cast
                using SizeofBinCast<T, auto N> = Base<(sizeof(Bin<N, N + 1>) as u8) + (sizeof(ExprAdd<N + 2>) as u16)>;

                // Cast ternary
                using CastTernary<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8) < N ? (sizeof(ExprAdd<N + 1>) as u8) : N>;
                using CastTernaryCmp<T, auto N> = Base<((sizeof(ExprAdd<N + 1>) as u8) > 5) ? 5 : (sizeof(ExprAdd<N + 1>) as u8)>;

                // Cast + sizeof without cast
                using CastPlusSizeof<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8) + sizeof(ExprAdd<N + 2>)>;

                // 3-level deep mixed sizeof + cast
                using DeepMixed<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8) + sizeof(Bin<N + 2, N + 3>) * (N + 4)>;

                // Cast to complex type (Bin)
                using CastToBin<T, auto N> = Base<(N + 1 as Bin<N, N + 1>)>;

                // Cast to Triplet
                using CastToTriplet<T, auto N> = Base<(N + 1 as Triplet<N, N + 1, N + 2>)>;

                // Cast + sizeof mixed
                using CastSizeofMixed<T, auto N> = Base<(N + 1 as Bin<N, N + 1>) + sizeof(ExprAdd<N + 2>)>;

                // Function call (1 arg)
                using FuncCall1<T, auto N> = Base<K(N + 1)>;

                // Function call (2 args)
                using FuncCall2<T, auto N> = Base<K(N + 1, N + 2)>;

                // Type-only parameter index (auto params must not consume type-param indices)
                using Named<X> = u8;
                using AutoFirst<auto N, T> = Named<T>;

                // parentParams must flow into type operator stringification
                using SizeofNamed<T> = Base<sizeof(Named<T>)>;

                // String literals keep their quotes
                using StrArg<auto N> = Base<K("hi")>;

                // Right operand parentheses must be preserved
                using PrecRight<auto A, auto B> = Base<(A - B) - (A - B)>;

                // Unary prefix formatting (parser represents these as 0 <op> expr)
                using NegExpr<auto N> = Base<(-N)>;
                using NotExpr<auto N> = Base<(!N)>;

                // Binary minus with an unsigned zero literal is not the parser's unary encoding
                using ZeroUMinus<auto N> = Base<0u - N>;

                // Unsigned literals keep their suffix
                using UnsignedSuffix<auto N> = Base<N + 4u>;

                // Right shift needs angle parentheses in template arguments
                using ShiftExpr<auto N> = Base<(N >> 2)>;

                // Endianness survives documentation stringification
                using BeAlias<T> = be u8;
                using BeParam<T> = be T;

                // Endian inside / outside template arguments
                using EndianArg = Alias<be u32>;
                using EndianOuter = be Alias<u32>;
                using EndianBoth = be Alias<be u32>;
                using EndianNested<T> = Alias<le Alias<be u8>>;

                // Attributes on type declarations
                using AttrAlias<T> = T [[deprecated]];
                using AttrMulti<T> = T [[deprecated, pack(1)]];
                struct AttrStruct { u8 x; } [[pack(1)]];
                union AttrUnion { u8 a; u16 b; } [[attr]];
                bitfield AttrBF { u8 x : 3; } [[attr2]];
                enum AttrEnum : u8 { A, B } [[maybe_unused]];
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &) const override {
            auto parseResult = m_runtime->parseString(getSourceCode(), "UsingDocs.pat");
            if (!parseResult.has_value()) {
                fmt::print("FAIL: Failed to parse docs test pattern\n");
                return false;
            }

            const auto &types = m_runtime->getInternals().parser->getTypes();

            auto check = [&](const char *typeName, const char *expected) -> bool {
                auto it = types.find(typeName);
                if (it == types.end()) {
                    fmt::print("FAIL: type '{}' not found\n", typeName);
                    return false;
                }
                std::string result = cli::sub::docs::generateTypeDocumentation(typeName, it->second.get());
                if (result.find(expected) == std::string::npos) {
                    fmt::print("FAIL: {} expected '{}', got:\n{}\n", typeName, expected, result);
                    return false;
                }
                return true;
            };

            bool ok = true;

            // Aliases
            ok &= check("Alias", "using Alias<T> = T;");
            ok &= check("AliasChain", "using AliasChain<T> = Alias<T>;");
            ok &= check("MultiAlias", "using MultiAlias<R, G, B, N> = Alias<R>;");
            ok &= check("AliasWrap", "using AliasWrap<T> = Alias<Alias<T>>;");

            // Base types
            ok &= check("Base", "using Base<auto N> = u8;");

            // Simple expressions
            ok &= check("ExprAdd", "using ExprAdd<auto N> = Base<N + 1>;");
            ok &= check("ExprAddVar", "using ExprAddVar<auto N> = Base<N + 1 + 5>;");

            // Comparisons
            ok &= check("ExprLt", "using ExprLt<auto N> = Base<(N < 5)>;");
            ok &= check("ExprGt", "using ExprGt<auto N> = Base<(N > 5)>;");

            // Function call
            ok &= check("ExprFunc", "using ExprFunc<auto N> = Base<K(N < 5)>;");

            // Ternary
            ok &= check("ExprTernary", "using ExprTernary<auto N> = Base<0 ? 1 : 2>;");

            // Evaluated sizeof (parser folds sizeof(builtin) to an unsigned literal before docs see it)
            ok &= check("ExprSizeofLit", "using ExprSizeofLit<auto N> = Base<4u>;");

            // Cast
            ok &= check("ExprCast", "using ExprCast<auto N> = Base<0 as u8>;");

            // Type operators
            ok &= check("ExprSizeofType", "using ExprSizeofType<T, auto N> = Base<sizeof(T)>;");
            ok &= check("ExprSizeofDollar", "using ExprSizeofDollar<T, auto N> = Base<sizeof($)>;");

            // Nested sizeof with expression
            ok &= check("NestedDeep", "using NestedDeep<T, auto N> = Base<sizeof(NestedBase<T, N - 1>) + N>;");

            // Multi-param / multi-type
            ok &= check("MultiParam", "using MultiParam<auto A, auto B, auto C, auto D> = Base<A + B + C + D>;");
            ok &= check("MultiType", "using MultiType<T, U, auto N> = Base<sizeof(T) + sizeof(U) + N>;");

            // Ternary with sizeof
            ok &= check("TernarySizeof", "using TernarySizeof<T, auto N> = Base<(sizeof(T) < N) ? sizeof(T) : N>;");
            ok &= check("TernaryNested", "using TernaryNested<T, auto N> = Base<(sizeof(T) < N) ? (sizeof(T) > 0 ? sizeof(T) : 1) : N>;");

            // Multi-type ternary
            ok &= check("TernaryMultiType", "using TernaryMultiType<T, U, auto N> = Base<(sizeof(T) + sizeof(U) + N < 100) ? sizeof(T) + sizeof(U) + N : 100>;");

            // Binary ops
            ok &= check("BinaryAdd", "using BinaryAdd<auto A, auto B> = Base<A + B>;");

            // Multi-op chains
            ok &= check("ChainOps", "using ChainOps<auto N> = Base<(N + 1) * (2 + 3)>;");

            // Ternary chains
            ok &= check("ChainTernary", "using ChainTernary<auto N> = Base<N == 0 ? 1 : (N == 1 ? 2 : 3)>;");

            // sizeof + auto
            ok &= check("SizeofAuto", "using SizeofAuto<T, auto N> = Base<sizeof(T) + N>;");

            // Two-type with auto math
            ok &= check("TwoTypeAuto", "using TwoTypeAuto<T, U, auto N> = Base<(sizeof(T) + N) * (sizeof(U) + N)>;");

            // Complex multi-param expression
            ok &= check("ComplexExpr", "using ComplexExpr<auto A, auto B> = Base<(A + B) * (A - B)>;");

            // Ternary with auto comparison
            ok &= check("TernaryAuto", "using TernaryAuto<auto A, auto B> = Base<(A < B) ? A : (B < A ? B : A + B)>;");

            // Deep ternary
            ok &= check("DeepTernary", "using DeepTernary<T, auto N> = Base<(sizeof(T) < N) ? (sizeof(T) > 0 ? sizeof(T) : 1) : (N > 0 ? N : 1)>;");

            // sizeof of type applications
            ok &= check("SizeofTypeApp", "using SizeofTypeApp<T, auto N> = Base<sizeof(ExprAdd<N + 1>)>;");
            ok &= check("SizeofTypeAppExpr", "using SizeofTypeAppExpr<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) + N) * (sizeof(ExprAdd<N + 2>) + N)>;");

            // Two-type sizeof comparison
            ok &= check("TwoTypeCompare", "using TwoTypeCompare<T, U, auto N> = Base<(sizeof(ExprAdd<N>) < sizeof(ExprAdd<N + 1>)) ? sizeof(ExprAdd<N>) : sizeof(ExprAdd<N + 1>)>;");

            // Cast + math
            ok &= check("CastMath", "using CastMath<T, auto N> = Base<N + 1 as u8>;");

            // Cast on sizeof
            ok &= check("CastSizeof", "using CastSizeof<T, auto N> = Base<sizeof(ExprAdd<N + 1>) as u8>;");
            ok &= check("CastSizeofMulti", "using CastSizeofMulti<T, auto N> = Base<sizeof(ExprAdd<N + 1>) as u8 + sizeof(ExprAdd<N + 2>) as u16>;");

            // sizeof of 2-param type app with cast
            ok &= check("SizeofBinCast", "using SizeofBinCast<T, auto N> = Base<sizeof(Bin<N, N + 1>) as u8 + sizeof(ExprAdd<N + 2>) as u16>;");

            // Cast ternary
            ok &= check("CastTernary", "using CastTernary<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8 < N) ? sizeof(ExprAdd<N + 1>) as u8 : N>;");
            ok &= check("CastTernaryCmp", "using CastTernaryCmp<T, auto N> = Base<(sizeof(ExprAdd<N + 1>) as u8 > 5) ? 5 : sizeof(ExprAdd<N + 1>) as u8>;");

            // Cast + sizeof without cast
            ok &= check("CastPlusSizeof", "using CastPlusSizeof<T, auto N> = Base<sizeof(ExprAdd<N + 1>) as u8 + sizeof(ExprAdd<N + 2>)>;");

            // 3-level deep mixed sizeof + cast
            ok &= check("DeepMixed", "using DeepMixed<T, auto N> = Base<sizeof(ExprAdd<N + 1>) as u8 + sizeof(Bin<N + 2, N + 3>) * (N + 4)>;");

            // Cast to complex type (Bin)
            ok &= check("CastToBin", "using CastToBin<T, auto N> = Base<N + 1 as Bin<N, N + 1>>;");

            // Cast to Triplet
            ok &= check("CastToTriplet", "using CastToTriplet<T, auto N> = Base<N + 1 as Triplet<N, N + 1, N + 2>>;");

            // Cast + sizeof mixed
            ok &= check("CastSizeofMixed", "using CastSizeofMixed<T, auto N> = Base<N + 1 as Bin<N, N + 1> + sizeof(ExprAdd<N + 2>)>;");

            // Function calls
            ok &= check("FuncCall1", "using FuncCall1<T, auto N> = Base<K(N + 1)>;");
            ok &= check("FuncCall2", "using FuncCall2<T, auto N> = Base<K(N + 1, N + 2)>;");

            // Type-only parameter index (auto params must not consume type-param indices)
            ok &= check("Named", "using Named<X> = u8;");
            ok &= check("AutoFirst", "using AutoFirst<auto N, T> = Named<T>;");

            // parentParams must flow into type operator stringification
            ok &= check("SizeofNamed", "using SizeofNamed<T> = Base<sizeof(Named<T>)>;");

            // String literals keep their quotes
            ok &= check("StrArg", "using StrArg<auto N> = Base<K(\"hi\")>;");

            // Right operand parentheses must be preserved
            ok &= check("PrecRight", "using PrecRight<auto A, auto B> = Base<A - B - (A - B)>;");

            // Unary prefix formatting (parser represents these as 0 <op> expr)
            ok &= check("NegExpr", "using NegExpr<auto N> = Base<-N>;");
            ok &= check("NotExpr", "using NotExpr<auto N> = Base<!N>;");

            // Binary minus with an unsigned zero literal is not the parser's unary encoding
            ok &= check("ZeroUMinus", "using ZeroUMinus<auto N> = Base<0u - N>;");

            // Unsigned literals keep their suffix
            ok &= check("UnsignedSuffix", "using UnsignedSuffix<auto N> = Base<N + 4u>;");

            // Right shift needs angle parentheses in template arguments
            ok &= check("ShiftExpr", "using ShiftExpr<auto N> = Base<(N >> 2)>;");

            // Endianness survives documentation stringification
            ok &= check("BeAlias", "using BeAlias<T> = be u8;");
            ok &= check("BeParam", "using BeParam<T> = be T;");

            // Endian inside / outside template arguments
            ok &= check("EndianArg", "using EndianArg = Alias<be u32>;");
            ok &= check("EndianOuter", "using EndianOuter = be Alias<u32>;");
            ok &= check("EndianBoth", "using EndianBoth = be Alias<be u32>;");
            ok &= check("EndianNested", "using EndianNested<T> = Alias<le Alias<be u8>>;");

            // Attributes on type declarations
            ok &= check("AttrAlias", "using AttrAlias<T> = T [[deprecated]];");
            ok &= check("AttrMulti", "using AttrMulti<T> = T [[deprecated, pack(1)]];");
            ok &= check("AttrStruct", "struct AttrStruct { ... } [[pack(1)]];");
            ok &= check("AttrUnion", "union AttrUnion { ... } [[attr]];");
            ok &= check("AttrBF", "bitfield AttrBF { ... } [[attr2]];");
            ok &= check("AttrEnum", "enum AttrEnum : u8 {\n    A,\n    B\n} [[maybe_unused]];");

            // Validate all types don't have ??? or empty args
            for (const auto &[typeName, typeNode] : types) {
                if (!typeNode->shouldDocument())
                    continue;
                try {
                    std::string result = cli::sub::docs::generateTypeDocumentation(typeName, typeNode.get());
                    if (result.empty()) {
                        fmt::print("FAIL: empty output for '{}'\n", typeName);
                        ok = false;
                    } else if (result.find("???") != std::string::npos) {
                        fmt::print("FAIL: ??? in output for '{}':\n{}\n", typeName, result);
                        ok = false;
                    } else if (result.find("<,") != std::string::npos || result.find(", >") != std::string::npos) {
                        fmt::print("FAIL: empty arg in output for '{}':\n{}\n", typeName, result);
                        ok = false;
                    }
                } catch (const std::exception &e) {
                    fmt::print("FAIL: exception for '{}': {}\n", typeName, e.what());
                    ok = false;
                }
            }

            // The parser can skip a statement without recording errors (auto parameter used in
            // type position), leaving an invalid TypeDecl behind. Documentation generation must
            // stay silent for it instead of crashing or emitting partial output.
            {
                const std::string invalidSource = R"(
                    using BadAuto<auto N> = NamedLate<N>;
                    using NamedLate<X> = u8;
                )";

                auto invalidParse = m_runtime->parseString(invalidSource, "UsingDocsBad.pat");
                if (!invalidParse.has_value()) {
                    fmt::print("FAIL: expected parse result for invalid alias\n");
                    ok = false;
                } else {
                    const auto &invalidTypes = m_runtime->getInternals().parser->getTypes();
                    auto it = invalidTypes.find("BadAuto");
                    if (it == invalidTypes.end()) {
                        fmt::print("FAIL: type 'BadAuto' not found\n");
                        ok = false;
                    } else {
                        auto result = cli::sub::docs::generateTypeDocumentation("BadAuto", it->second.get());
                        if (!result.empty()) {
                            fmt::print("FAIL: expected empty documentation for invalid type 'BadAuto', got:\n{}\n", result);
                            ok = false;
                        }
                    }
                }
            }

            return ok;
        }
    };

}
