#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <vector>
#include <string>
#include <variant>

#include <fmt/args.h>

namespace pl::lib::libstd::libstd {

    namespace {

        std::string format(const auto &params) {
            using namespace pl::core;

            auto format = params[0].toString(true);
            std::string message;

            fmt::dynamic_format_arg_store<fmt::format_context> formatArgs;

            for (u32 i = 1; i < params.size(); i++) {
                auto &param = params[i];

                std::visit(wolv::util::overloaded {
                    [&](ptrn::Pattern *value) {
                        formatArgs.push_back(value->toString());
                    },
                    [&](const std::string &value) {
                        formatArgs.push_back(value.c_str());
                    },
                    [&](auto &&value) {
                        formatArgs.push_back(value);
                    }
                },  param);
            }

            try {
                return fmt::vformat(format, formatArgs);
            } catch (fmt::format_error &error) {
                err::E0012.throwError(fmt::format("Error while formatting: {}", error.what()));
            }
        }

    }

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        pl::api::Namespace nsStd = { "builtin", "std" };
        {
            /* print(format, args...) */
            runtime.addFunction(nsStd, "print", FunctionParameterCount::moreThan(0), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                ctx->getConsole().log(LogConsole::Level::Info, format(params));

                return std::nullopt;
            });

            /* format(format, args...) */
            runtime.addFunction(nsStd, "format", FunctionParameterCount::moreThan(0), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return format(params);
            });

            /* env(name) */
            runtime.addFunction(nsStd, "env", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto name = params[0].toString(false);

                auto env = ctx->getEnvVariable(name);
                if (env)
                    return env;
                else {
                    ctx->getConsole().log(LogConsole::Level::Warning, fmt::format("environment variable '{}' does not exist", name));
                    return "";
                }
            });

            /* pack_size(...) */
            runtime.addFunction(nsStd, "sizeof_pack", FunctionParameterCount::atLeast(0), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                return u128(params.size());
            });

            /* error(message) */
            runtime.addFunction(nsStd, "error", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                err::E0012.throwError(params[0].toString(true));

                return std::nullopt;
            });

            /* warning(message) */
            runtime.addFunction(nsStd, "warning", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                ctx->getConsole().log(LogConsole::Level::Warning, params[0].toString(true));

                return std::nullopt;
            });
        }


    }

}