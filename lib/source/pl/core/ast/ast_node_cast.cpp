#include <pl/core/ast/ast_node_cast.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <bit>

namespace pl::core::ast {

    namespace {

        template<typename T>
        T changeEndianess(Evaluator *evaluator, T value, size_t size, std::endian endian) {
            if (endian == evaluator->getDefaultEndian())
                return value;

            if constexpr (std::endian::native == std::endian::little)
                return hlp::changeEndianess(value, size, std::endian::big);
            else
                return hlp::changeEndianess(value, size, std::endian::little);
        }

        template<typename To1, typename To2>
        To1 doIntegerCast(Evaluator *evaluator, auto value, const std::shared_ptr<ptrn::Pattern> &typePattern) {
            To2 result = changeEndianess(evaluator, To2(value), typePattern->getSize(), typePattern->getEndian());

            return result;
        }

    }

    std::unique_ptr<ASTNode> ASTNodeCast::castValue(const Token::Literal &literal, Token::ValueType type, const std::shared_ptr<ptrn::Pattern> &typePattern, Evaluator *evaluator) const {
        return std::unique_ptr<ASTNode>(std::visit(wolv::util::overloaded {
            [&, this](const std::shared_ptr<ptrn::Pattern> &value) -> ASTNode * {
                if (value->hasAttribute("transform"))
                    return castValue(value->getValue(), type, typePattern, evaluator).release();
                else
                    return new ASTNodeLiteral(value->getValue());
            },
            [&, this](const std::string &value) -> ASTNode * {
                if (Token::isUnsigned(type)) {
                    if (value.size() > sizeof(u128))
                        err::E0004.throwError(fmt::format("Cannot cast value of type 'str' of size {} to type '{}' of size {}.", value.size(), Token::getTypeName(type), Token::getTypeSize(type)), {}, this->getLocation());
                    u128 result = 0;
                    std::memcpy(&result, value.data(), value.size());

                    auto endianAdjustedValue = changeEndianess(evaluator, result & hlp::bitmask(Token::getTypeSize(type) * 8), value.size(), typePattern->getEndian());
                    return new ASTNodeLiteral(endianAdjustedValue);
                } else
                    err::E0004.throwError(fmt::format("Cannot cast value of type 'str' to type '{}'.", Token::getTypeName(type)), {}, this->getLocation());
            },
            [&, this](auto &&value) -> ASTNode * {
                switch (type) {
                    case Token::ValueType::Unsigned8Bit:
                        return new ASTNodeLiteral(doIntegerCast<u128, u8>(evaluator, value, typePattern));
                    case Token::ValueType::Unsigned16Bit:
                        return new ASTNodeLiteral(doIntegerCast<u128, u16>(evaluator, value, typePattern));
                    case Token::ValueType::Unsigned32Bit:
                        return new ASTNodeLiteral(doIntegerCast<u128, u32>(evaluator, value, typePattern));
                    case Token::ValueType::Unsigned64Bit:
                        return new ASTNodeLiteral(doIntegerCast<u128, u64>(evaluator, value, typePattern));
                    case Token::ValueType::Unsigned128Bit:
                        return new ASTNodeLiteral(doIntegerCast<u128, u128>(evaluator, value, typePattern));
                    case Token::ValueType::Signed8Bit:
                        return new ASTNodeLiteral(doIntegerCast<i128, i8>(evaluator, value, typePattern));
                    case Token::ValueType::Signed16Bit:
                        return new ASTNodeLiteral(doIntegerCast<i128, i16>(evaluator, value, typePattern));
                    case Token::ValueType::Signed32Bit:
                        return new ASTNodeLiteral(doIntegerCast<i128, i32>(evaluator, value, typePattern));
                    case Token::ValueType::Signed64Bit:
                        return new ASTNodeLiteral(doIntegerCast<i128, i64>(evaluator, value, typePattern));
                    case Token::ValueType::Signed128Bit:
                        return new ASTNodeLiteral(doIntegerCast<i128, i128>(evaluator, value, typePattern));
                    case Token::ValueType::Float:
                        return new ASTNodeLiteral(doIntegerCast<double, float>(evaluator, value, typePattern));
                    case Token::ValueType::Double:
                        return new ASTNodeLiteral(doIntegerCast<double, double>(evaluator, value, typePattern));
                    case Token::ValueType::Character:
                        return new ASTNodeLiteral(doIntegerCast<char, char>(evaluator, value, typePattern));
                    case Token::ValueType::Character16:
                        return new ASTNodeLiteral(doIntegerCast<u128, char16_t>(evaluator, value, typePattern));
                    case Token::ValueType::Boolean:
                        return new ASTNodeLiteral(doIntegerCast<bool, bool>(evaluator, value, typePattern));
                    case Token::ValueType::String:
                    {
                        std::string string(sizeof(value), '\x00');
                        std::memcpy(string.data(), &value, string.size());

                        // Remove trailing null bytes
                        if (auto pos = string.find('\x00'); pos != std::string::npos)
                            string.erase(pos);

                        if (typePattern->getEndian() != std::endian::native)
                            std::reverse(string.begin(), string.end());

                        return new ASTNodeLiteral(string);
                    }
                    default:
                        err::E0004.throwError(fmt::format("Cannot cast value of type '{}' to type '{}'.", typePattern->getTypeName(), Token::getTypeName(type)), {}, this->getLocation());
                }
            },
        },
        literal));
    }

    ASTNodeCast::ASTNodeCast(std::unique_ptr<ASTNode> &&value, std::unique_ptr<ASTNode> &&type, bool reinterpret) : m_value(std::move(value)), m_type(std::move(type)), m_reinterpret(reinterpret) { }

    ASTNodeCast::ASTNodeCast(const ASTNodeCast &other) : ASTNode(other) {
        this->m_value = other.m_value->clone();
        this->m_type  = other.m_type->clone();
        this->m_reinterpret = other.m_reinterpret;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeCast::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();
        ON_SCOPE_EXIT { evaluator->setBitwiseReadOffset(startOffset); };

        evaluator->pushSectionId(ptrn::Pattern::InstantiationSectionId);
        ON_SCOPE_EXIT { evaluator->popSectionId(); };

        auto evaluatedValue = this->m_value->evaluate(evaluator);
        auto evaluatedType  = this->m_type->evaluate(evaluator);

        auto literal = dynamic_cast<ASTNodeLiteral *>(evaluatedValue.get());
        if (literal == nullptr)
            err::E0004.throwError("Cannot use void expression in a cast.", {}, this->getLocation());

        std::vector<std::shared_ptr<ptrn::Pattern>> typePatterns;
        this->m_type->createPatterns(evaluator, typePatterns);
        if (typePatterns.empty())
            err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());

        auto &typePattern = typePatterns.front();

        auto value = literal->getValue();

        if (!m_reinterpret) {
            auto type = dynamic_cast<ASTNodeBuiltinType *>(evaluatedType.get())->getType();

            value = std::visit(wolv::util::overloaded {
                [&evaluator, &type](const std::shared_ptr<ptrn::Pattern> &value) -> Token::Literal {
                    if (Token::isInteger(type) && value->getSize() <= Token::getTypeSize(type)) {
                        u128 result = 0;
                        evaluator->readData(value->getOffset(), &result, value->getSize(), value->getSection());

                        return result;
                    } else {
                        return value;
                    }
                },
                [](auto &value) -> Token::Literal { return value; }
            }, value);

            return castValue(value, type, typePattern, evaluator);
        } else {
            auto bytes = value.toBytes();

            if (bytes.size() != typePattern->getSize()) {
                err::E0004.throwError("Types of both sides in reinterpret expression need to have the same size.", fmt::format("Left side is {} bytes, Right side is {} bytes", bytes.size(), typePattern->getSize()), this->getLocation());
            }

            typePattern->setLocal(true);

            return std::make_unique<ASTNodeLiteral>(typePattern);
        }
    }

}