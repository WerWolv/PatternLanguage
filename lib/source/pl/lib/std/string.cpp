#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <vector>
#include <string>
#include <variant>

#include <fmt/args.h>

#include <wolv/utils/charconv.hpp>

namespace pl::lib::libstd::string {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdString = { "builtin", "std", "string" };
        {
            /* length(string) */
            runtime.addFunction(nsStdString, "length", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = params[0].toString(false);

                return u128(string.length());
            });

            /* at(string, index) */
            runtime.addFunction(nsStdString, "at", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = params[0].toString(false);
                auto index  = i64(params[1].toSigned());

                // Calculate the absolute value of the index
                const auto signIndex = index >> (sizeof(index) * 8 - 1);
                const auto absIndex  = (index ^ signIndex) - signIndex;

                if (absIndex >= intptr_t(string.length()))
                    err::E0012.throwError(fmt::format("Character index {} out of range of string '{}' with length {}.", absIndex, string, string.length()));

                if (index >= 0)
                    return char(string[index]);
                else
                    return char(string[string.length() - -index]);
            });

            /* substr(string, pos, count) */
            runtime.addFunction(nsStdString, "substr", FunctionParameterCount::exactly(3), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = params[0].toString(false);
                auto pos    = u64(params[1].toUnsigned());
                auto size   = u64(params[2].toUnsigned());

                if (pos > string.length())
                    err::E0012.throwError(fmt::format("The starting position {} out of range for string '{}' with length {}.", pos, string, string.length()));
                if (pos+size > string.length())
                    err::E0012.throwError(fmt::format("The ending position {} out of range for string '{}' with length {}.", pos+size, string, string.length()));

                return string.substr(pos, size);
            });

            /* parse_int(string, base) */
            runtime.addFunction(nsStdString, "parse_int", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = params[0].toString(false);
                auto base   = u64(params[1].toUnsigned());

                if constexpr (std::integral<i128>)
                    return wolv::util::from_chars<i128>(string, base).value_or(0);
                else
                    return wolv::util::from_chars<i64>(string, base).value_or(0);

            });

            /* parse_float(string) */
            runtime.addFunction(nsStdString, "parse_float", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto string = params[0].toString(false);

                return wolv::util::from_chars<double>(string).value_or(0.0);
            });
        }

    }

}
