#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

namespace pl::core::ast {

    class ASTNodeUnion : public ASTNode,
                         public Attributable {
    public:
        ASTNodeUnion() = default;
        ASTNodeUnion(const ASTNodeUnion &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeUnion(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getMembers() const { return this->m_members; }
        void addMember(std::shared_ptr<ASTNode> &&node) { this->m_members.push_back(std::move(node)); }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_members;
    };

}