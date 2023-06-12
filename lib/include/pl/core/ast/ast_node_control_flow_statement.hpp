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

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

    private:
        ControlFlowStatement m_type;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}