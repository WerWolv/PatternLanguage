#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <vector>
#include <string>
#include <variant>

#include <fmt/args.h>

namespace pl::lib::libstd::string {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdString = { "builtin", "std", "string" };
        {
            /* length(string) */
            runtime.addFunction(nsStdString, "length", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);

                return u128(string.length());
            });

            /* at(string, index) */
            runtime.addFunction(nsStdString, "at", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);
                auto index  = Token::literalToSigned(params[1]);

#if defined(OS_MACOS)
                const auto signIndex = index >> (sizeof(index) * 8 - 1);
                const auto absIndex  = (index ^ signIndex) - signIndex;
#else
                const auto absIndex = std::abs(index);
#endif

                if (absIndex > string.length())
                    err::E0012.throwError(fmt::format("Character index {} out of range of string '{}' with length {}.", absIndex, string, string.length()));

                if (index >= 0)
                    return char(string[index]);
                else
                    return char(string[string.length() - -index]);
            });

            /* substr(string, pos, count) */
            runtime.addFunction(nsStdString, "substr", FunctionParameterCount::exactly(3), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);
                auto pos    = Token::literalToUnsigned(params[1]);
                auto size   = Token::literalToUnsigned(params[2]);

                if (pos > string.length())
                    err::E0012.throwError(fmt::format("Character index {} out of range of string '{}' with length {}.", pos, string, string.length()));

                return string.substr(pos, size);
            });

            /* parse_int(string, base) */
            runtime.addFunction(nsStdString, "parse_int", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);
                auto base   = Token::literalToUnsigned(params[1]);

                return i128(std::strtoll(string.c_str(), nullptr, base));
            });

            /* parse_float(string) */
            runtime.addFunction(nsStdString, "parse_float", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = Token::literalToString(params[0], false);

                return double(std::strtod(string.c_str(), nullptr));
            });
        }

    }

}