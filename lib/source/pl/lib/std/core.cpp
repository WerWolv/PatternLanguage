#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/lib/std/types.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_enum.hpp>

#include <vector>
#include <string>


namespace pl::lib::libstd::core {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdCore = { "builtin", "std", "core" };
        {
            /* has_attribute(pattern, attribute_name) */
            runtime.addFunction(nsStdCore, "has_attribute", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();
                auto attributeName = params[1].toString(false);

                auto &attributes = pattern->getAttributes();
                if (attributes == nullptr)
                    return false;
                else
                    return attributes->contains(attributeName);
            });

            /* get_attribute_argument(pattern, attribute_name, index) */
            runtime.addFunction(nsStdCore, "get_attribute_argument", FunctionParameterCount::exactly(3), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();
                auto attributeName = params[1].toString(false);
                auto index = params[2].toUnsigned();

                auto &attributes = pattern->getAttributes();
                if (attributes == nullptr || !attributes->contains(attributeName))
                    return std::string();
                else
                    return attributes->at(attributeName)[index];
            });

            /* set_pattern_color(pattern, color) */
            runtime.addFunction(nsStdCore, "set_pattern_color", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();
                auto color = params[1].toUnsigned();

                pattern->setColor(color);

                return std::nullopt;
            });

            /* set_display_name(pattern, name) */
            runtime.addFunction(nsStdCore, "set_display_name", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();
                auto name = params[1].toString(false);

                pattern->setDisplayName(name);

                return std::nullopt;
            });

            /* set_pattern_comment(pattern, comment) */
            runtime.addFunction(nsStdCore, "set_pattern_comment", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();
                auto comment = params[1].toString(false);

                pattern->setComment(comment);

                return std::nullopt;
            });

            /* set_endian(endian) */
            runtime.addFunction(nsStdCore, "set_endian", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                types::Endian endian = params[0].toUnsigned();

                ctx->setDefaultEndian(endian);

                return std::nullopt;
            });

            /* get_endian() -> endian */
            runtime.addFunction(nsStdCore, "get_endian", FunctionParameterCount::none(), [](Evaluator *ctx, auto) -> std::optional<Token::Literal> {
                switch (ctx->getDefaultEndian()) {
                    case std::endian::big:
                        return u128(1);
                    case std::endian::little:
                        return u128(2);
                }

                return std::nullopt;
            });

            /* array_index() -> index */
            runtime.addFunction(nsStdCore, "array_index", FunctionParameterCount::none(), [](Evaluator *ctx, auto) -> std::optional<Token::Literal> {
                auto index = ctx->getCurrentArrayIndex();

                if (index.has_value())
                    return u128(*index);
                else
                    return u128(0);
            });

            /* member_count(pattern) -> count */
            runtime.addFunction(nsStdCore, "member_count", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();

                if (auto iterable = dynamic_cast<ptrn::IIterable*>(pattern.get()); iterable != nullptr)
                    return u128(iterable->getEntryCount());
                else
                    return u128(0);
            });

            /* has_member(pattern, name) -> member_exists */
            runtime.addFunction(nsStdCore, "has_member", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();
                auto name = params[1].toString(false);

                auto hasMember = [&](const auto &members) {
                    return std::any_of(members.begin(), members.end(),
                        [&](const std::shared_ptr<ptrn::Pattern> &member) {
                            return member->getVariableName() == name;
                        });
                };

                if (auto iterable = dynamic_cast<ptrn::IIterable*>(pattern.get()); iterable != nullptr)
                    return hasMember(iterable->getEntries());
                else
                    return false;
            });

            /* formatted_value(pattern) -> str */
            runtime.addFunction(nsStdCore, "formatted_value", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();

                return pattern->getFormattedValue();
            });

            /* is_valid_enum(pattern) -> bool */
            runtime.addFunction(nsStdCore, "is_valid_enum", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();

                if (auto enumPattern = dynamic_cast<ptrn::PatternEnum*>(pattern.get()); enumPattern != nullptr) {
                    auto value = enumPattern->getValue().toUnsigned();
                    for (auto &[name, entry] : enumPattern->getEnumValues()) {
                        auto min = entry.min.toUnsigned();
                        auto max = entry.max.toUnsigned();

                        if (value >= min && value <= max)
                            return true;
                    }
                }

                return false;
            });

            /* execute_function(function_name, args...) -> bool */
            runtime.addFunction(nsStdCore, "execute_function", FunctionParameterCount::atLeast(1), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
                auto functionName = params[0].toString();

                auto function = evaluator->findFunction(functionName);
                if (!function.has_value()) {
                    err::E0009.throwError(fmt::format("Function '{}' does not exist.", functionName), {});
                }

                std::vector<pl::core::Token::Literal> functionParams;
                for (size_t i = 1; i < params.size(); i += 1)
                    functionParams.emplace_back(std::move(params[i]));

                return function.value().func(evaluator, functionParams);
            });

            /* insert_pattern(pattern) */
            runtime.addFunction(nsStdCore, "insert_pattern", FunctionParameterCount::exactly(1), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();

                auto &currScope = *evaluator->getScope(0).scope;
                if (auto iterable = dynamic_cast<ptrn::IIterable*>(pattern.get()); iterable != nullptr && pattern->getTypeName().empty()) {
                    auto entries = iterable->getEntries();

                    // Check for duplicates
                    for (auto &a : entries) {
                        for (auto &b : currScope) {
                            if (a->getVariableName() == b->getVariableName()) [[unlikely]] {
                                err::E0012.throwError(fmt::format("Error inserting patterns into current scope. Pattern with name '{}' already exists.", a->getVariableName()));
                            }
                        }
                    }

                    std::move(entries.begin(), entries.end(), std::back_inserter(currScope));
                } else {
                    currScope.emplace_back(pattern->clone());
                }

                return std::nullopt;
            });


            runtime.addFunction(nsStdCore, "set_pattern_palette_colors", FunctionParameterCount::moreThan(0), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
                std::vector<u32> colors;
                for (const auto &param : params) {
                    auto value = param.toUnsigned();
                    if (value > std::numeric_limits<u32>::max())
                        err::E0012.throwError(fmt::format("Invalid color value: 0x{:08X}", value));

                    colors.emplace_back(value);
                }

                evaluator->setPatternColorPalette(colors);

                return std::nullopt;
            });

            runtime.addFunction(nsStdCore, "reset_pattern_palette", FunctionParameterCount::none(), [](Evaluator *evaluator, auto) -> std::optional<Token::Literal> {
                evaluator->resetPatternColorPaletteIndex();
                return std::nullopt;
            });
        }
    }

}