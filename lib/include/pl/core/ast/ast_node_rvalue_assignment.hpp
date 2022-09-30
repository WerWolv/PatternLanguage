#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    class ASTNodeRValueAssignment : public ASTNode {
    public:
        ASTNodeRValueAssignment(std::unique_ptr<ASTNode> &&lvalue, std::unique_ptr<ASTNode> &&rvalue) : m_lvalue(std::move(lvalue)), m_rvalue(std::move(rvalue)) {
        }

        ASTNodeRValueAssignment(const ASTNodeRValueAssignment &other) : ASTNode(other) {
            this->m_lvalue = other.m_lvalue->clone();
            this->m_rvalue = other.m_rvalue->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeRValueAssignment(*this));
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getLValue() const {
            return this->m_lvalue;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getRValue() const {
            return this->m_rvalue;
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            this->execute(evaluator);

            return {};
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            const auto lhs     = this->getLValue()->createPatterns(evaluator);
            const auto rhs     = this->getRValue()->evaluate(evaluator);

            if (lhs.empty())
                err::E0003.throwError("Cannot find variable in this scope.", {}, this);

            auto &pattern = lhs.front();
            const auto literal = dynamic_cast<ASTNodeLiteral *>(rhs.get());

            evaluator->setVariable(pattern.get(), literal->getValue());

            return {};
        }

    private:
        std::unique_ptr<ASTNode> m_lvalue, m_rvalue;
    };

}