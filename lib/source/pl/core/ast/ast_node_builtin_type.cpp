#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

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
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto size   = Token::getTypeSize(this->m_type);
        auto offset = evaluator->getReadOffsetAndIncrement(size);

        std::unique_ptr<ptrn::Pattern> pattern;
        if (Token::isUnsigned(this->m_type))
            pattern = std::make_unique<ptrn::PatternUnsigned>(evaluator, offset, size, getLocation().line);
        else if (Token::isSigned(this->m_type))
            pattern = std::make_unique<ptrn::PatternSigned>(evaluator, offset, size, getLocation().line);
        else if (Token::isFloatingPoint(this->m_type))
            pattern = std::make_unique<ptrn::PatternFloat>(evaluator, offset, size, getLocation().line);
        else if (this->m_type == Token::ValueType::Boolean)
            pattern = std::make_unique<ptrn::PatternBoolean>(evaluator, offset, getLocation().line);
        else if (this->m_type == Token::ValueType::Character)
            pattern = std::make_unique<ptrn::PatternCharacter>(evaluator, offset, getLocation().line);
        else if (this->m_type == Token::ValueType::Character16)
            pattern = std::make_unique<ptrn::PatternWideCharacter>(evaluator, offset, getLocation().line);
        else if (this->m_type == Token::ValueType::Padding)
            pattern = std::make_unique<ptrn::PatternPadding>(evaluator, offset, 1, getLocation().line);
        else if (this->m_type == Token::ValueType::String)
            pattern = std::make_unique<ptrn::PatternString>(evaluator, offset, 0, getLocation().line);
        else if (this->m_type == Token::ValueType::CustomType) {
            std::vector<Token::Literal> params;

            const auto &templateParams = evaluator->getTemplateParameters();
            for (u32 i = templateParams.size() - m_parameterCount.min; i < templateParams.size(); i += 1) {
                params.emplace_back(templateParams[i]->getValue());
            }

            pattern = m_customTypeCallback(evaluator, params);

            // The size of the custom built-in type is only known now so we need to increment the cursor here manually again
            std::ignore = evaluator->getReadOffsetAndIncrement(pattern->getSize());
        }
        else if (this->m_type == Token::ValueType::Auto)
            return { };
        else
            err::E0001.throwError("Invalid builtin type.", {}, this->getLocation());

        pattern->setTypeName(Token::getTypeName(this->m_type));

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

}