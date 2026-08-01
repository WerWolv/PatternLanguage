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

}
