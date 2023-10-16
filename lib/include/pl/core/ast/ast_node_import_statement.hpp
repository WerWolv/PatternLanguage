#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeImport : public ASTNode {
    public:
        ASTNodeImport() = default;
        ASTNodeImport(const ASTNodeImport &other);

        ASTNodeImport(std::vector<ast::ASTNode> importedStatements) : m_importedStatements(std::move(importedStatements)) { }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeImport(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

    private:
        std::vector<ast::ASTNode> m_importedStatements;

    };

}