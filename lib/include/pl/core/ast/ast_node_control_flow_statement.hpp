#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeControlFlowStatement : public ASTNode {
    public:
        explicit ASTNodeControlFlowStatement(ControlFlowStatement type, std::unique_ptr<ASTNode> &&rvalue);
        ASTNodeControlFlowStatement(const ASTNodeControlFlowStatement &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeControlFlowStatement(*this));
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

        [[nodiscard]] ControlFlowStatement getType() const {
            return this->m_type;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getRValue() {
            return m_rvalue;
        }

        void accept(vis::ASTVisitor &v) override {
            v.visit(*this);
        }
    private:
        ControlFlowStatement m_type;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}