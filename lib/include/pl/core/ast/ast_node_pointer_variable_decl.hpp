#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_type_appilication.hpp>

namespace pl::core::ast {

    class ASTNodeTypeDecl;

    class ASTNodePointerVariableDecl : public ASTNode,
                                       public Attributable {
    public:
        ASTNodePointerVariableDecl(std::string name, std::shared_ptr<ASTNode> type, std::shared_ptr<ASTNodeTypeApplication> sizeType, std::unique_ptr<ASTNode> &&placementOffset = nullptr, std::unique_ptr<ASTNode> &&placementSection = nullptr);
        ASTNodePointerVariableDecl(const ASTNodePointerVariableDecl &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodePointerVariableDecl(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNode> &getType() const { return this->m_type; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNodeTypeApplication> &getSizeType() const { return this->m_sizeType; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getPlacementOffset() const { return this->m_placementOffset; }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;

    private:
        std::string m_name;
        std::shared_ptr<ASTNode> m_type;
        std::shared_ptr<ASTNodeTypeApplication> m_sizeType;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;
    };

}
