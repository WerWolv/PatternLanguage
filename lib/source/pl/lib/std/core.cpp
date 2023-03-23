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
                        return 1;
                    case std::endian::little:
                        return 2;
                }

                return std::nullopt;
            });

            /* set_bitfield_order(order) */
            runtime.addFunction(nsStdCore, "set_bitfield_order", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto endian = params[0].toUnsigned();

                switch (endian) {
                    case 0:
                        ctx->setBitfieldOrder(pl::core::BitfieldOrder::LeftToRight);
                        break;
                    case 1:
                        ctx->setBitfieldOrder(pl::core::BitfieldOrder::RightToLeft);
                        break;
                    case 2:
                        ctx->setBitfieldOrder({});
                        break;
                    default:
                        err::E0012.throwError("Invalid endian value.", "Try one of the values in the std::core::BitfieldOrder enum.");
                }

                return std::nullopt;
            });

            /* get_bitfield_order() -> order */
            runtime.addFunction(nsStdCore, "get_bitfield_order", FunctionParameterCount::none(), [](Evaluator *ctx, auto) -> std::optional<Token::Literal> {
                if (ctx->getBitfieldOrder().has_value()) {
                    switch (ctx->getBitfieldOrder().value()) {
                        case pl::core::BitfieldOrder::LeftToRight:
                            return 0;
                        case pl::core::BitfieldOrder::RightToLeft:
                            return 1;
                    }
                }

                return 2;
            });

            /* array_index() -> index */
            runtime.addFunction(nsStdCore, "array_index", FunctionParameterCount::none(), [](Evaluator *ctx, auto) -> std::optional<Token::Literal> {
                auto index = ctx->getCurrentArrayIndex();

                if (index.has_value())
                    return u128(*index);
                else
                    return 0;
            });

            /* member_count(pattern) -> count */
            runtime.addFunction(nsStdCore, "member_count", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = params[0].toPattern();

                if (auto iteratable = dynamic_cast<ptrn::Iteratable*>(pattern); iteratable != nullptr)
                    return u128(iteratable->getEntryCount());
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

                if (auto iteratable = dynamic_cast<ptrn::Iteratable*>(pattern); iteratable != nullptr)
                    return hasMember(iteratable->getEntries());
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

                if (auto enumPattern = dynamic_cast<ptrn::PatternEnum*>(pattern); enumPattern != nullptr) {
                    auto value = enumPattern->getValue().toUnsigned();
                    for (auto &entry : enumPattern->getEnumValues()) {
                        auto min = entry.min.toUnsigned();
                        auto max = entry.max.toUnsigned();

                        if (value >= min && value <= max)
                            return true;
                    }
                }

                return false;
            });
        }
    }

}