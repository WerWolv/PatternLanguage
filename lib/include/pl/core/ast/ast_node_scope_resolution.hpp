#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

namespace pl::core::ast {

    class ASTNodeScopeResolution : public ASTNode {
    public:
        explicit ASTNodeScopeResolution(std::shared_ptr<ASTNodeTypeDecl> &&type, std::string name);
        ASTNodeScopeResolution(const ASTNodeScopeResolution &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeScopeResolution(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

        [[nodiscard]] const std::shared_ptr<ASTNodeTypeDecl> &getType() {
            return m_type;
        }
        [[nodiscard]] const std::string &getName() {
            return m_name;
        }

        void accept(vis::ASTVisitor &v) override {
            v.visit(*this);
        }

    private:
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::string m_name;
    };

}