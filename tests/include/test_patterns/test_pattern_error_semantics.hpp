#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternFailingSemantic : public TestPattern {
    public:
        TestPatternFailingSemantic(core::Evaluator *evaluator, const std::string &name, std::string source)
            : TestPattern(evaluator, name, Mode::Failing), m_source(std::move(source)) { }

        [[nodiscard]] std::string getSourceCode() const override {
            return m_source;
        }

    private:
        std::string m_source;
    };

    class TestPatternDivisionByZeroFail : public TestPatternFailingSemantic {
    public:
        TestPatternDivisionByZeroFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "DivisionByZeroFail", R"(
                u32 zero = 0;
                u32 result = 100 / zero;
            )") { }
    };

    class TestPatternModuloByZeroFail : public TestPatternFailingSemantic {
    public:
        TestPatternModuloByZeroFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "ModuloByZeroFail", R"(
                u32 zero = 0;
                u32 result = 100 % zero;
            )") { }
    };

    class TestPatternArrayOutOfBoundsFail : public TestPatternFailingSemantic {
    public:
        TestPatternArrayOutOfBoundsFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "ArrayOutOfBoundsFail", R"(
                u32 values[2] = { 10, 20 };
                u32 result = values[2];
            )") { }
    };

    class TestPatternConstAssignmentFail : public TestPatternFailingSemantic {
    public:
        TestPatternConstAssignmentFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "ConstAssignmentFail", R"(
                const u32 value = 10;
                value = 20;
            )") { }
    };

    class TestPatternAmbiguousMatchFail : public TestPatternFailingSemantic {
    public:
        TestPatternAmbiguousMatchFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "AmbiguousMatchFail", R"(
                fn classify(auto value) {
                    match (value) {
                        (1 ... 5): return 1;
                        (3 ... 7): return 2;
                        (_): return 0;
                    }
                };

                classify(4);
            )") { }
    };

    class TestPatternNegativeStringRepeatFail : public TestPatternFailingSemantic {
    public:
        TestPatternNegativeStringRepeatFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "NegativeStringRepeatFail", R"(
                str result = "value" * -1;
            )") { }
    };

    class TestPatternInvalidStringOperandFail : public TestPatternFailingSemantic {
    public:
        TestPatternInvalidStringOperandFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "InvalidStringOperandFail", R"(
                str result = "left" - "right";
            )") { }
    };

    class TestPatternTooFewArgumentsFail : public TestPatternFailingSemantic {
    public:
        TestPatternTooFewArgumentsFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "TooFewArgumentsFail", R"(
                fn identity(auto value) {
                    return value;
                };

                identity();
            )") { }
    };

    class TestPatternUndefinedFunctionFail : public TestPatternFailingSemantic {
    public:
        TestPatternUndefinedFunctionFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "UndefinedFunctionFail", R"(
                function_does_not_exist();
            )") { }
    };

    class TestPatternDuplicateVariableFail : public TestPatternFailingSemantic {
    public:
        TestPatternDuplicateVariableFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "DuplicateVariableFail", R"(
                u32 value = 1;
                u32 value = 2;
            )") { }
    };

    class TestPatternStaticArrayRangeOverflowFail : public TestPatternFailingSemantic {
    public:
        TestPatternStaticArrayRangeOverflowFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "StaticArrayRangeOverflowFail", R"(
                u64 wrappedSize = 0xFFFFFFFFFFFFFFFF;
                u8 values[wrappedSize] @ 0;
            )") { }
    };

    class TestPatternVarRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternVarRedeclarationFail(core::Evaluator *evaluator)
            : TestPatternFailingSemantic(evaluator, "VarRedeclarationFail", R"(
                u8 x;
                u8 x;
            )") { }
    };
//    class TestPatternVarRedeclarationInFuncFail: public TestPatternFailingSemantic {
//    public:
//        TestPatternVarRedeclarationInFuncFail(core::Evaluator *evaluator)
//        : TestPatternFailingSemantic(evaluator, "VarRedeclarationInFuncFail", R"(
//                fn f() {
//                    u8 x;
//                    u8 x;
//                };
//            )") { }
//    };
    class TestPatternArrayVarRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternArrayVarRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "ArrayVarRedeclarationFail", R"(
                u8 x[2];
                u8 x[2];
            )") { }
    };
    class TestPatternPointerVarRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternPointerVarRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "PointerVarRedeclarationFail", R"(
                u8* x: u8 @ 0;
                u8* x: u8 @ 0;
            )") { }
    };
    class TestPatternMultiVarRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternMultiVarRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "MultiVarRedeclarationFail", R"(
                struct S {
                    u8 x, x;
                };
            )") { }
    };
//    class TestPatternMultiVarMixRedeclarationFail: public TestPatternFailingSemantic {
//    public:
//        TestPatternMultiVarMixRedeclarationFail(core::Evaluator *evaluator)
//        : TestPatternFailingSemantic(evaluator, "MultiVarMixRedeclarationFail", R"(
//                struct S {
//                    u8 x;
//                    u8 x, y;
//                };
//            )") { }
//    };
    class TestPatternStructMemberRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternStructMemberRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "StructMemberRedeclarationFail", R"(
                struct S {
                    u8 x;
                    u8 x;
                };
            )") { }
    };
    class TestPatternUnionMemberRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternUnionMemberRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "UnionMemberRedeclarationFail", R"(
                union S {
                    u8 x;
                    u8 x;
                };
            )") { }
    };
//    class TestPatternEnumMemberRedeclarationFail: public TestPatternFailingSemantic {
//    public:
//        TestPatternEnumMemberRedeclarationFail(core::Evaluator *evaluator)
//        : TestPatternFailingSemantic(evaluator, "EnumMemberRedeclarationFail", R"(
//                enum E : u8 {
//                    a = 1,
//                    a = 2,
//                };
//            )") { }
//    };
    class TestPatternBitfieldMemberRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternBitfieldMemberRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "BitfieldMemberRedeclarationFail", R"(
                bitfield B {
                    x: 5;
                    x: 11;
                };
            )") { }
    };

//    class TestPatternBitfieldArrayMemberRedeclarationFail: public TestPatternFailingSemantic {
//    public:
//        TestPatternBitfieldArrayMemberRedeclarationFail(core::Evaluator *evaluator)
//        : TestPatternFailingSemantic(evaluator, "BitfieldArrayMemberRedeclarationFail", R"(
//                bitfield B {
//                    x : 4;
//                    y : 4;
//                };
//                bitfield TestBitfield {
//                    a : 2;
//                    B a[2];
//                };
//            )") { }
//    };
    class TestPatternTemplateParamRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternTemplateParamRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "TemplateParamRedeclarationFail", R"(
                struct S<T, T> {};
            )") { }
    };
    class TestPatternNonTypeTemplateParamRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternNonTypeTemplateParamRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "NonTypeTemplateParamRedeclarationFail", R"(
                struct S<auto x, auto x> {};
            )") { }
    };
    // TODO: validate match statements
    // TODO: validate both func-like and struct-like scopes for correctness

    class TestPatternFuncRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternFuncRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "FuncRedeclarationFail", R"(
                fn f() {};
                fn f() {};
            )") { }
    };

    class TestPatternFuncParamRedeclarationFail: public TestPatternFailingSemantic {
    public:
        TestPatternFuncParamRedeclarationFail(core::Evaluator *evaluator)
        : TestPatternFailingSemantic(evaluator, "FuncParamRedeclarationFail", R"(
                fn f(u8 x, u8 x) {};
            )") { }
    };

//    class TestPatternRedeclarationInFuncBodyFail: public TestPatternFailingSemantic {
//    public:
//        TestPatternRedeclarationInFuncBodyFail(core::Evaluator *evaluator)
//        : TestPatternFailingSemantic(evaluator, "RedeclarationInFuncBodyFail", R"(
//                fn f() {
//                   u8 x;
//                   u8 x;
//                };
//            )") { }
//    };

//    class TestPatternFuncParamAndBodyRedeclarationFail: public TestPatternFailingSemantic {
//    public:
//        TestPatternFuncParamAndBodyRedeclarationFail(core::Evaluator *evaluator)
//        : TestPatternFailingSemantic(evaluator, "FuncParamAndBodyRedeclarationFail", R"(
//                fn f(u8 x) {
//                   u8 x;
//                };
//            )") { }
//    };

    // class TestPatternGlobalContinueFail : public TestPatternFailingSemantic {
    // public:
    //     TestPatternGlobalContinueFail(core::Evaluator *evaluator)
    //     : TestPatternFailingSemantic(evaluator, "GlobalContinueFail", R"(
    //             continue;
    //         )") { }
    // };
    // class TestPatternFunctionContinueFail : public TestPatternFailingSemantic {
    // public:
    //     TestPatternFunctionContinueFail(core::Evaluator *evaluator)
    //     : TestPatternFailingSemantic(evaluator, "FunctionContinueFail", R"(
    //             fn f() { continue; };
    //         )") { }
    // };
    // class TestPatternGlobalBreakFail : public TestPatternFailingSemantic {
    // public:
    //     TestPatternGlobalBreakFail(core::Evaluator *evaluator)
    //     : TestPatternFailingSemantic(evaluator, "GlobalBreakFail", R"(
    //             break;
    //         )") { }
    // };
    // class TestPatternFunctionBreakFail : public TestPatternFailingSemantic {
    // public:
    //     TestPatternFunctionBreakFail(core::Evaluator *evaluator)
    //     : TestPatternFailingSemantic(evaluator, "FunctionBreakFail", R"(
    //             fn f() { break; };
    //         )") { }
    // };
}
