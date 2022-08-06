#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::lib::libstd::math {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdMath = { "builtin", "std", "math" };
        {
            /* floor(value) */
            runtime.addFunction(nsStdMath, "floor", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::floor(Token::literalToFloatingPoint(params[0]));
            });

            /* ceil(value) */
            runtime.addFunction(nsStdMath, "ceil", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::ceil(Token::literalToFloatingPoint(params[0]));
            });

            /* round(value) */
            runtime.addFunction(nsStdMath, "round", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::round(Token::literalToFloatingPoint(params[0]));
            });

            /* trunc(value) */
            runtime.addFunction(nsStdMath, "trunc", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::trunc(Token::literalToFloatingPoint(params[0]));
            });


            /* log10(value) */
            runtime.addFunction(nsStdMath, "log10", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::log10(Token::literalToFloatingPoint(params[0]));
            });

            /* log2(value) */
            runtime.addFunction(nsStdMath, "log2", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::log2(Token::literalToFloatingPoint(params[0]));
            });

            /* ln(value) */
            runtime.addFunction(nsStdMath, "ln", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::log(Token::literalToFloatingPoint(params[0]));
            });


            /* fmod(x, y) */
            runtime.addFunction(nsStdMath, "fmod", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::fmod(Token::literalToFloatingPoint(params[0]), Token::literalToFloatingPoint(params[1]));
            });

            /* pow(base, exp) */
            runtime.addFunction(nsStdMath, "pow", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::pow(Token::literalToFloatingPoint(params[0]), Token::literalToFloatingPoint(params[1]));
            });

            /* sqrt(value) */
            runtime.addFunction(nsStdMath, "sqrt", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::sqrt(Token::literalToFloatingPoint(params[0]));
            });

            /* cbrt(value) */
            runtime.addFunction(nsStdMath, "cbrt", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::cbrt(Token::literalToFloatingPoint(params[0]));
            });


            /* sin(value) */
            runtime.addFunction(nsStdMath, "sin", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::sin(Token::literalToFloatingPoint(params[0]));
            });

            /* cos(value) */
            runtime.addFunction(nsStdMath, "cos", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::cos(Token::literalToFloatingPoint(params[0]));
            });

            /* tan(value) */
            runtime.addFunction(nsStdMath, "tan", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::tan(Token::literalToFloatingPoint(params[0]));
            });

            /* asin(value) */
            runtime.addFunction(nsStdMath, "asin", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::asin(Token::literalToFloatingPoint(params[0]));
            });

            /* acos(value) */
            runtime.addFunction(nsStdMath, "acos", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::acos(Token::literalToFloatingPoint(params[0]));
            });

            /* atan(value) */
            runtime.addFunction(nsStdMath, "atan", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::atan(Token::literalToFloatingPoint(params[0]));
            });

            /* atan2(y, x) */
            runtime.addFunction(nsStdMath, "atan", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::atan2(Token::literalToFloatingPoint(params[0]), Token::literalToFloatingPoint(params[1]));
            });


            /* sinh(value) */
            runtime.addFunction(nsStdMath, "sinh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::sinh(Token::literalToFloatingPoint(params[0]));
            });

            /* cosh(value) */
            runtime.addFunction(nsStdMath, "cosh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::cosh(Token::literalToFloatingPoint(params[0]));
            });

            /* tanh(value) */
            runtime.addFunction(nsStdMath, "tanh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::tanh(Token::literalToFloatingPoint(params[0]));
            });

            /* asinh(value) */
            runtime.addFunction(nsStdMath, "asinh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::asinh(Token::literalToFloatingPoint(params[0]));
            });

            /* acosh(value) */
            runtime.addFunction(nsStdMath, "acosh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::acosh(Token::literalToFloatingPoint(params[0]));
            });

            /* atanh(value) */
            runtime.addFunction(nsStdMath, "atanh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::atanh(Token::literalToFloatingPoint(params[0]));
            });
        }
    }

}