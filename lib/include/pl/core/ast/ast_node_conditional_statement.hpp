#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeConditionalStatement : public ASTNode {
    public:
        explicit ASTNodeConditionalStatement(std::unique_ptr<ASTNode> condition, std::vector<std::unique_ptr<ASTNode>> &&trueBody, std::vector<std::unique_ptr<ASTNode>> &&falseBody);
        ASTNodeConditionalStatement(const ASTNodeConditionalStatement &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeConditionalStatement(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

        [[nodiscard]] const std::unique_ptr<ASTNode> &getCondition() const {
            return this->m_condition;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getTrueBody() const {
            return this->m_trueBody;
        }
        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getFalseBody() const {
            return this->m_falseBody;
        }

    private:
        [[nodiscard]] bool evaluateCondition(const std::unique_ptr<ASTNode> &condition, Evaluator *evaluator) const;

        std::unique_ptr<ASTNode> m_condition;
        std::vector<std::unique_ptr<ASTNode>> m_trueBody, m_falseBody;
    };

}