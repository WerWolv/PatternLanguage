#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_type_appilication.hpp>

namespace pl::core::ast {

    class ASTNodeTypeDecl;

    class ASTNodeArrayVariableDecl : public ASTNode,
                                     public Attributable {
    public:
        ASTNodeArrayVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeApplication> type, std::unique_ptr<ASTNode> &&size, std::unique_ptr<ASTNode> &&placementOffset = {}, std::unique_ptr<ASTNode> &&placementSection = {}, bool constant = false);
        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeArrayVariableDecl(*this));
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

        [[nodiscard]] const std::string &getName() const {
            return this->m_name;
        }

        [[nodiscard]] const std::shared_ptr<ASTNodeTypeApplication> &getType() const {
            return this->m_type;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getSize() const {
            return this->m_size;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getPlacementOffset() const {
            return this->m_placementOffset;
        }

        [[nodiscard]] bool isConstant() const {
            return this->m_constant;
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeApplication> m_type;
        std::unique_ptr<ASTNode> m_size;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;
        bool m_constant;

        void createStaticArray(Evaluator *evaluator, std::shared_ptr<ptrn::Pattern> &resultPattern) const;
        void createDynamicArray(Evaluator *evaluator, std::shared_ptr<ptrn::Pattern> &resultPattern) const;
    };

}