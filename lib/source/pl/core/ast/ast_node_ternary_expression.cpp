#include <pl/core/ast/ast_node_ternary_expression.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeTernaryExpression::ASTNodeTernaryExpression(std::unique_ptr<ASTNode> &&first, std::unique_ptr<ASTNode> &&second, std::unique_ptr<ASTNode> &&third, Token::Operator op)
    : ASTNode(), m_first(std::move(first)), m_second(std::move(second)), m_third(std::move(third)), m_operator(op) { }

    ASTNodeTernaryExpression::ASTNodeTernaryExpression(const ASTNodeTernaryExpression &other) : ASTNode(other) {
        this->m_operator = other.m_operator;
        this->m_first    = other.m_first->clone();
        this->m_second   = other.m_second->clone();
        this->m_third    = other.m_third->clone();
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeTernaryExpression::evaluate(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        if (this->getFirstOperand() == nullptr || this->getSecondOperand() == nullptr || this->getThirdOperand() == nullptr)
            err::E0002.throwError("Void expression used in ternary expression.", "If you used a function for one of the operands, make sure it returned a value.", this);

        auto conditionNode  = this->getFirstOperand()->evaluate(evaluator);

        auto *conditionValue  = dynamic_cast<ASTNodeLiteral *>(conditionNode.get());

        if (conditionValue == nullptr)
            err::E0010.throwError("Cannot use void expression in ternary expression.", {}, this);

        auto condition = std::visit(wolv::util::overloaded {
                [](const std::string &value) -> bool { return !value.empty(); },
                [this](const std::shared_ptr<ptrn::Pattern> &pattern) -> bool { err::E0002.throwError(fmt::format("Cannot cast {} to bool.", pattern->getTypeName()), {}, this); },
                [](auto &&value) -> bool { return bool(value); }
        }, conditionValue->getValue());

        if (condition) {
            return this->getSecondOperand()->evaluate(evaluator);
        } else {
            return this->getThirdOperand()->evaluate(evaluator);
        }
    }

}