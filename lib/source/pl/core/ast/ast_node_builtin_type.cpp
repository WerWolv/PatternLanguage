#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_string.hpp>

namespace pl::core::ast {

    ASTNodeBuiltinType::ASTNodeBuiltinType(Token::ValueType type)
    : ASTNode(), m_type(type) { }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeBuiltinType::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto size   = Token::getTypeSize(this->m_type);
        auto offset = evaluator->getReadOffsetAndIncrement(size);

        std::unique_ptr<ptrn::Pattern> pattern;
        if (Token::isUnsigned(this->m_type))
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternUnsigned(evaluator, offset, size));
        else if (Token::isSigned(this->m_type))
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternSigned(evaluator, offset, size));
        else if (Token::isFloatingPoint(this->m_type))
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternFloat(evaluator, offset, size));
        else if (this->m_type == Token::ValueType::Boolean)
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternBoolean(evaluator, offset));
        else if (this->m_type == Token::ValueType::Character)
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternCharacter(evaluator, offset));
        else if (this->m_type == Token::ValueType::Character16)
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternWideCharacter(evaluator, offset));
        else if (this->m_type == Token::ValueType::Padding)
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternPadding(evaluator, offset, 1));
        else if (this->m_type == Token::ValueType::String)
            pattern = std::unique_ptr<ptrn::Pattern>(new ptrn::PatternString(evaluator, offset, 0));
        else if (this->m_type == Token::ValueType::Auto)
            return { };
        else
            err::E0001.throwError("Invalid builtin type.", {}, this);

        pattern->setTypeName(Token::getTypeName(this->m_type));

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

}