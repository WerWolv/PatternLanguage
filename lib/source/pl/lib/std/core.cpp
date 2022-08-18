#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

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

            /* set_endian(endian) */
            runtime.addFunction(nsStdCore, "set_endian", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto endian = Token::literalToUnsigned(params[0]);

                switch (endian) {
                    case 0:
                        ctx->setDefaultEndian(std::endian::native);
                        break;
                    case 1:
                        ctx->setDefaultEndian(std::endian::big);
                        break;
                    case 2:
                        ctx->setDefaultEndian(std::endian::little);
                        break;
                    default:
                        err::E0012.throwError("Invalid endian value.", "Try one of the values in the std::core::Endian enum.");
                }

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
        }
    }

}