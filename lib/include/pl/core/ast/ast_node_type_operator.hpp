#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeTypeOperator : public ASTNode {
    public:
        ASTNodeTypeOperator(Token::Operator op, std::unique_ptr<ASTNode> &&expression);
        explicit ASTNodeTypeOperator(Token::Operator op);
        ASTNodeTypeOperator(const ASTNodeTypeOperator &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeOperator(*this));
        }

        [[nodiscard]] Token::Operator getOperator() const {
            return this->m_op;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getExpression() const {
            return this->m_expression;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

    private:
        Token::Operator m_op;
        std::unique_ptr<ASTNode> m_expression;

        bool m_providerOperation = false;
    };

}