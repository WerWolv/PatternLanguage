#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    class ASTNodeWhileStatement : public ASTNode {
    public:
        explicit ASTNodeWhileStatement(std::unique_ptr<ASTNode> &&condition, std::vector<std::unique_ptr<ASTNode>> &&body, std::unique_ptr<ASTNode> &&postExpression = nullptr);

        ASTNodeWhileStatement(const ASTNodeWhileStatement &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeWhileStatement(*this));
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getCondition() const {
            return this->m_condition;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getBody() const {
            return this->m_body;
        }

        FunctionResult execute(Evaluator *evaluator) const override;

        [[nodiscard]] bool evaluateCondition(Evaluator *evaluator) const;

    private:
        std::unique_ptr<ASTNode> m_condition;
        std::vector<std::unique_ptr<ASTNode>> m_body;
        std::unique_ptr<ASTNode> m_postExpression;
    };

}