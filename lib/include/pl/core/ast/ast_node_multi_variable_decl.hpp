#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeMultiVariableDecl : public ASTNode {
    public:
        explicit ASTNodeMultiVariableDecl(std::vector<std::shared_ptr<ASTNode>> &&variables);
        ASTNodeMultiVariableDecl(const ASTNodeMultiVariableDecl &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMultiVariableDecl(*this));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getVariables() {
            return this->m_variables;
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

    private:
        std::vector<std::shared_ptr<ASTNode>> m_variables;
    };

}