#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    class ASTNodeLValueAssignment : public ASTNode {
    public:
        ASTNodeLValueAssignment(std::string lvalueName, std::unique_ptr<ASTNode> &&rvalue) : m_lvalueName(std::move(lvalueName)), m_rvalue(std::move(rvalue)) {
        }

        ASTNodeLValueAssignment(const ASTNodeLValueAssignment &other) : ASTNode(other) {
            this->m_lvalueName = other.m_lvalueName;

            if (other.m_rvalue != nullptr)
                this->m_rvalue     = other.m_rvalue->clone();
        }

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

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            this->execute(evaluator);

            return {};
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            const auto node    = this->getRValue()->evaluate(evaluator);
            const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());

            if (this->getLValueName() == "$")
                evaluator->dataOffset() = Token::literalToUnsigned(literal->getValue());
            else
                evaluator->setVariable(this->getLValueName(), literal->getValue());

            return {};
        }

    private:
        std::string m_lvalueName;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}