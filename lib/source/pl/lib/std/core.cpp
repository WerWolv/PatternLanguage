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
                auto pattern = Token::literalToPattern(params[0]);
                auto attributeName = Token::literalToString(params[1], false);

                auto &attributes = pattern->getAttributes();
                if (attributes == nullptr)
                    return false;
                else
                    return attributes->contains(attributeName);
            });

            /* get_attribute_value(pattern, attribute_name) */
            runtime.addFunction(nsStdCore, "get_attribute_value", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = Token::literalToPattern(params[0]);
                auto attributeName = Token::literalToString(params[1], false);

                auto &attributes = pattern->getAttributes();
                if (attributes == nullptr || !attributes->contains(attributeName))
                    return std::string();
                else
                    return attributes->at(attributeName);
            });

            /* set_pattern_color(pattern, color) */
            runtime.addFunction(nsStdCore, "set_pattern_color", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = Token::literalToPattern(params[0]);
                auto color = Token::literalToUnsigned(params[1]);

                pattern->setColor(color);

                return std::nullopt;
            });

            /* set_pattern_comment(pattern, comment) */
            runtime.addFunction(nsStdCore, "set_pattern_comment", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = Token::literalToPattern(params[0]);
                auto comment = Token::literalToString(params[1], false);

                pattern->setComment(comment);

                return std::nullopt;
            });

            /* set_endian(endian) */
            runtime.addFunction(nsStdCore, "set_endian", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                types::Endian endian = Token::literalToUnsigned(params[0]);

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
                auto endian = Token::literalToUnsigned(params[0]);

                switch (endian) {
                    case 0:
                        ctx->setBitfieldOrder(pl::core::BitfieldOrder::LeftToRight);
                        break;
                    case 1:
                        ctx->setBitfieldOrder(pl::core::BitfieldOrder::RightToLeft);
                        break;
                    default:
                        err::E0012.throwError("Invalid endian value.", "Try one of the values in the std::core::BitfieldOrder enum.");
                }

                return std::nullopt;
            });

            /* get_bitfield_order() -> order */
            runtime.addFunction(nsStdCore, "get_bitfield_order", FunctionParameterCount::none(), [](Evaluator *ctx, auto) -> std::optional<Token::Literal> {
                switch (ctx->getBitfieldOrder()) {
                    case pl::core::BitfieldOrder::LeftToRight:
                        return 0;
                    case pl::core::BitfieldOrder::RightToLeft:
                        return 1;
                }

                return std::nullopt;
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
                auto pattern = Token::literalToPattern(params[0]);

                if (auto structPattern = dynamic_cast<ptrn::PatternStruct*>(pattern))
                    return u128(structPattern->getMembers().size());
                else if (auto unionPattern = dynamic_cast<ptrn::PatternUnion*>(pattern))
                    return u128(unionPattern->getMembers().size());
                else if (auto bitfieldPattern = dynamic_cast<ptrn::PatternBitfield*>(pattern))
                    return u128(bitfieldPattern->getFields().size());
                else if (auto dynamicArrayPattern = dynamic_cast<ptrn::PatternArrayDynamic*>(pattern))
                    return u128(dynamicArrayPattern->getEntryCount());
                else if (auto staticArrayPattern = dynamic_cast<ptrn::PatternArrayStatic*>(pattern))
                    return u128(staticArrayPattern->getEntryCount());
                else
                    return u128(0);
            });

            /* has_member(pattern, name) -> member_exists */
            runtime.addFunction(nsStdCore, "has_member", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = Token::literalToPattern(params[0]);
                auto name = Token::literalToString(params[1], false);

                auto hasMember = [&](const auto &members) {
                    return std::any_of(members.begin(), members.end(),
                        [&](const std::shared_ptr<ptrn::Pattern> &member) {
                            return member->getVariableName() == name;
                        });
                };

                if (auto structPattern = dynamic_cast<ptrn::PatternStruct*>(pattern))
                    return hasMember(structPattern->getMembers());
                else if (auto unionPattern = dynamic_cast<ptrn::PatternUnion*>(pattern))
                    return hasMember(unionPattern->getMembers());
                else if (auto bitfieldPattern = dynamic_cast<ptrn::PatternBitfield*>(pattern))
                    return hasMember(bitfieldPattern->getFields());
                else
                    return false;
            });

            /* formatted_value(pattern) -> str */
            runtime.addFunction(nsStdCore, "formatted_value", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto pattern = Token::literalToPattern(params[0]);

                return pattern->getFormattedValue();
            });
        }
    }

}