#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeTernaryExpression : public ASTNode {
    public:
        ASTNodeTernaryExpression(std::unique_ptr<ASTNode> &&first, std::unique_ptr<ASTNode> &&second, std::unique_ptr<ASTNode> &&third, Token::Operator op);
        ASTNodeTernaryExpression(const ASTNodeTernaryExpression &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTernaryExpression(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

        [[nodiscard]] const std::unique_ptr<ASTNode> &getFirstOperand() const { return this->m_first; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getSecondOperand() const { return this->m_second; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getThirdOperand() const { return this->m_third; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

    private:
        std::unique_ptr<ASTNode> m_first, m_second, m_third;
        Token::Operator m_operator;
    };

}