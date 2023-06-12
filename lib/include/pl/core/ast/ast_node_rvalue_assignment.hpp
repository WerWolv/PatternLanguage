#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeRValueAssignment : public ASTNode {
    public:
        ASTNodeRValueAssignment(std::unique_ptr<ASTNode> &&lvalue, std::unique_ptr<ASTNode> &&rvalue);
        ASTNodeRValueAssignment(const ASTNodeRValueAssignment &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeRValueAssignment(*this));
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getLValue() const {
            return this->m_lvalue;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getRValue() const {
            return this->m_rvalue;
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

    private:
        std::unique_ptr<ASTNode> m_lvalue, m_rvalue;
    };

}