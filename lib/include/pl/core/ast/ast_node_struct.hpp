#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

namespace pl::core::ast {

    class ASTNodeStruct : public ASTNode,
                          public Attributable {
    public:
        ASTNodeStruct() = default;
        ASTNodeStruct(const ASTNodeStruct &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeStruct(*this));
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getMembers() const { return this->m_members; }
        void addMember(std::shared_ptr<ASTNode> &&node) { this->m_members.push_back(std::move(node)); }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getInheritance() const { return this->m_inheritance; }
        void addInheritance(std::shared_ptr<ASTNode> &&node) { this->m_inheritance.push_back(std::move(node)); }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_members;
        std::vector<std::shared_ptr<ASTNode>> m_inheritance;
    };

}