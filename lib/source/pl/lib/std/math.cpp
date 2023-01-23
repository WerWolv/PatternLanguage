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
                return std::floor(params[0].toFloatingPoint());
            });

            /* ceil(value) */
            runtime.addFunction(nsStdMath, "ceil", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::ceil(params[0].toFloatingPoint());
            });

            /* round(value) */
            runtime.addFunction(nsStdMath, "round", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::round(params[0].toFloatingPoint());
            });

            /* trunc(value) */
            runtime.addFunction(nsStdMath, "trunc", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::trunc(params[0].toFloatingPoint());
            });


            /* log10(value) */
            runtime.addFunction(nsStdMath, "log10", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::log10(params[0].toFloatingPoint());
            });

            /* log2(value) */
            runtime.addFunction(nsStdMath, "log2", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::log2(params[0].toFloatingPoint());
            });

            /* ln(value) */
            runtime.addFunction(nsStdMath, "ln", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::log(params[0].toFloatingPoint());
            });


            /* fmod(x, y) */
            runtime.addFunction(nsStdMath, "fmod", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::fmod(params[0].toFloatingPoint(), params[1].toFloatingPoint());
            });

            /* pow(base, exp) */
            runtime.addFunction(nsStdMath, "pow", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::pow(params[0].toFloatingPoint(), params[1].toFloatingPoint());
            });

            /* sqrt(value) */
            runtime.addFunction(nsStdMath, "sqrt", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::sqrt(params[0].toFloatingPoint());
            });

            /* cbrt(value) */
            runtime.addFunction(nsStdMath, "cbrt", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::cbrt(params[0].toFloatingPoint());
            });


            /* sin(value) */
            runtime.addFunction(nsStdMath, "sin", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::sin(params[0].toFloatingPoint());
            });

            /* cos(value) */
            runtime.addFunction(nsStdMath, "cos", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::cos(params[0].toFloatingPoint());
            });

            /* tan(value) */
            runtime.addFunction(nsStdMath, "tan", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::tan(params[0].toFloatingPoint());
            });

            /* asin(value) */
            runtime.addFunction(nsStdMath, "asin", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::asin(params[0].toFloatingPoint());
            });

            /* acos(value) */
            runtime.addFunction(nsStdMath, "acos", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::acos(params[0].toFloatingPoint());
            });

            /* atan(value) */
            runtime.addFunction(nsStdMath, "atan", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::atan(params[0].toFloatingPoint());
            });

            /* atan2(y, x) */
            runtime.addFunction(nsStdMath, "atan", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::atan2(params[0].toFloatingPoint(), params[1].toFloatingPoint());
            });


            /* sinh(value) */
            runtime.addFunction(nsStdMath, "sinh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::sinh(params[0].toFloatingPoint());
            });

            /* cosh(value) */
            runtime.addFunction(nsStdMath, "cosh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::cosh(params[0].toFloatingPoint());
            });

            /* tanh(value) */
            runtime.addFunction(nsStdMath, "tanh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::tanh(params[0].toFloatingPoint());
            });

            /* asinh(value) */
            runtime.addFunction(nsStdMath, "asinh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::asinh(params[0].toFloatingPoint());
            });

            /* acosh(value) */
            runtime.addFunction(nsStdMath, "acosh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::acosh(params[0].toFloatingPoint());
            });

            /* atanh(value) */
            runtime.addFunction(nsStdMath, "atanh", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return std::atanh(params[0].toFloatingPoint());
            });
        }
    }

}