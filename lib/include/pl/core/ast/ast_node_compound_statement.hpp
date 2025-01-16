#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <vector>

namespace pl::core::ast {

    class ASTNodeCompoundStatement : public ASTNode,
                                     public Attributable {
    public:
        explicit ASTNodeCompoundStatement(std::vector<std::shared_ptr<ASTNode>> &&statements, bool newScope = false);
        explicit ASTNodeCompoundStatement(std::vector<std::unique_ptr<ASTNode>> &&statements, bool newScope = false);
        ASTNodeCompoundStatement(const ASTNodeCompoundStatement &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeCompoundStatement(*this));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>>& getStatements() const { return this->m_statements; }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;
        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        FunctionResult execute(Evaluator *evaluator) const override;
        void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) override;

    public:
        std::vector<std::shared_ptr<ASTNode>> m_statements;
        bool m_newScope = false;
    };

}