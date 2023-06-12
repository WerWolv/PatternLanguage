#pragma once

#include <pl/core/ast/ast_node.hpp>

#include <pl/core/ast/ast_node_attribute.hpp>

namespace pl::core::ast {

    class ASTNodeLValueAssignment : public ASTNode, public Attributable {
    public:
        ASTNodeLValueAssignment(std::string lvalueName, std::unique_ptr<ASTNode> &&rvalue);
        ASTNodeLValueAssignment(const ASTNodeLValueAssignment &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeLValueAssignment(*this));
        }

        [[nodiscard]] const std::string &getLValueName() const {
            return this->m_lvalueName;
        }

        void setLValueName(const std::string &lvalueName) {
            this->m_lvalueName = lvalueName;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getRValue() const {
            return this->m_rvalue;
        }

        void setRValue(std::unique_ptr<ASTNode> &&rvalue) {
            this->m_rvalue = std::move(rvalue);
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

    private:
        std::string m_lvalueName;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}