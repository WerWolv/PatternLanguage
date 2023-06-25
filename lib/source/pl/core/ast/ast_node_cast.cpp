#include <pl/core/ast/ast_node_cast.hpp>

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
                        err::E0004.throwError(fmt::format("Cannot cast value of type 'str' of size {} to type '{}' of size {}.", value.size(), Token::getTypeName(type), Token::getTypeSize(type)), {}, this);
                    u128 result = 0;
                    std::memcpy(&result, value.data(), value.size());

                    auto endianAdjustedValue = changeEndianess(evaluator, result & hlp::bitmask(Token::getTypeSize(type) * 8), value.size(), typePattern->getEndian());
                    return new ASTNodeLiteral(endianAdjustedValue);
                } else
                    err::E0004.throwError(fmt::format("Cannot cast value of type 'str' to type '{}'.", Token::getTypeName(type)), {}, this);
            },
            [&, this](auto &&value) -> ASTNode * {
                auto endianAdjustedValue = changeEndianess(evaluator, value, typePattern->getSize(), typePattern->getEndian());
                switch (type) {
                    case Token::ValueType::Unsigned8Bit:
                        return new ASTNodeLiteral(u128(u8(endianAdjustedValue)));
                    case Token::ValueType::Unsigned16Bit:
                        return new ASTNodeLiteral(u128(u16(endianAdjustedValue)));
                    case Token::ValueType::Unsigned32Bit:
                        return new ASTNodeLiteral(u128(u32(endianAdjustedValue)));
                    case Token::ValueType::Unsigned64Bit:
                        return new ASTNodeLiteral(u128(u64(endianAdjustedValue)));
                    case Token::ValueType::Unsigned128Bit:
                        return new ASTNodeLiteral(u128(endianAdjustedValue));
                    case Token::ValueType::Signed8Bit:
                        return new ASTNodeLiteral(i128(i8(endianAdjustedValue)));
                    case Token::ValueType::Signed16Bit:
                        return new ASTNodeLiteral(i128(i16(endianAdjustedValue)));
                    case Token::ValueType::Signed32Bit:
                        return new ASTNodeLiteral(i128(i32(endianAdjustedValue)));
                    case Token::ValueType::Signed64Bit:
                        return new ASTNodeLiteral(i128(i64(endianAdjustedValue)));
                    case Token::ValueType::Signed128Bit:
                        return new ASTNodeLiteral(i128(endianAdjustedValue));
                    case Token::ValueType::Float:
                        return new ASTNodeLiteral(double(float(endianAdjustedValue)));
                    case Token::ValueType::Double:
                        return new ASTNodeLiteral(double(endianAdjustedValue));
                    case Token::ValueType::Character:
                        return new ASTNodeLiteral(char(endianAdjustedValue));
                    case Token::ValueType::Character16:
                        return new ASTNodeLiteral(u128(char16_t(endianAdjustedValue)));
                    case Token::ValueType::Boolean:
                        return new ASTNodeLiteral(bool(endianAdjustedValue));
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
                        err::E0004.throwError(fmt::format("Cannot cast value of type '{}' to type '{}'.", typePattern->getTypeName(), Token::getTypeName(type)), {}, this);
                }
            },
        },
        literal));
    }

    ASTNodeCast::ASTNodeCast(std::unique_ptr<ASTNode> &&value, std::unique_ptr<ASTNode> &&type) : m_value(std::move(value)), m_type(std::move(type)) { }

    ASTNodeCast::ASTNodeCast(const ASTNodeCast &other) : ASTNode(other) {
        this->m_value = other.m_value->clone();
        this->m_type  = other.m_type->clone();
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeCast::evaluate(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();
        ON_SCOPE_EXIT { evaluator->setBitwiseReadOffset(startOffset); };

        auto evaluatedValue = this->m_value->evaluate(evaluator);
        auto evaluatedType  = this->m_type->evaluate(evaluator);

        auto literal = dynamic_cast<ASTNodeLiteral *>(evaluatedValue.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot use void expression in a cast.", {}, this);

        auto type = dynamic_cast<ASTNodeBuiltinType *>(evaluatedType.get())->getType();

        auto typePatterns = this->m_type->createPatterns(evaluator);
        if (typePatterns.empty())
            err::E0005.throwError("'auto' can only be used with parameters.", { }, this);

        auto &typePattern = typePatterns.front();

        auto value = literal->getValue();

        value = std::visit(wolv::util::overloaded {
                [&](const std::shared_ptr<ptrn::Pattern> &value) -> Token::Literal {
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
    }

}