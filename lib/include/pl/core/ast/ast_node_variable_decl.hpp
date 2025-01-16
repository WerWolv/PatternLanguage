#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

namespace pl::core::ast {

    class ASTNodeVariableDecl : public ASTNode,
                                public Attributable {
    public:
        ASTNodeVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&placementOffset = nullptr, std::unique_ptr<ASTNode> &&placementSection = nullptr, bool inVariable = false, bool outVariable = false, bool constant = false);

        ASTNodeVariableDecl(const ASTNodeVariableDecl &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeVariableDecl(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNodeTypeDecl> &getType() const { return this->m_type; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] constexpr bool isInVariable() const { return this->m_inVariable; }
        [[nodiscard]] constexpr bool isOutVariable() const { return this->m_outVariable; }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;

        FunctionResult execute(Evaluator *evaluator) const override;

        [[nodiscard]] bool isConstant() const {
            return this->m_constant;
        }

    private:
        u128 evaluatePlacementOffset(Evaluator *evaluator) const;
        u64 evaluatePlacementSection(Evaluator *evaluator) const;

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;

        bool m_inVariable = false, m_outVariable = false;
        bool m_constant = false;
    };

}