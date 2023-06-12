#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeScopeResolution : public ASTNode {
    public:
        explicit ASTNodeScopeResolution(std::shared_ptr<ASTNode> &&type, std::string name);
        ASTNodeScopeResolution(const ASTNodeScopeResolution &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeScopeResolution(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

    private:
        std::shared_ptr<ASTNode> m_type;
        std::string m_name;
    };

}