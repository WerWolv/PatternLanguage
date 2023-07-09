#include <pl/core/ast/ast_node_lvalue_assignment.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeLValueAssignment::ASTNodeLValueAssignment(std::string lvalueName, std::unique_ptr<ASTNode> &&rvalue) : m_lvalueName(std::move(lvalueName)), m_rvalue(std::move(rvalue)) {
    }

    ASTNodeLValueAssignment::ASTNodeLValueAssignment(const ASTNodeLValueAssignment &other) : ASTNode(other), Attributable(other) {
        this->m_lvalueName = other.m_lvalueName;

        if (other.m_rvalue != nullptr)
            this->m_rvalue     = other.m_rvalue->clone();
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeLValueAssignment::createPatterns(Evaluator *evaluator) const {
        this->execute(evaluator);

        return {};
    }

    ASTNode::FunctionResult ASTNodeLValueAssignment::execute(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        const auto node    = this->getRValue()->evaluate(evaluator);
        const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot assign void expression to variable.", {}, this);


        if (this->getLValueName() == "$")
            evaluator->setReadOffset(literal->getValue().toUnsigned());
        else {
            auto variable = evaluator->getVariableByName(this->getLValueName());
            applyVariableAttributes(evaluator, this, variable);

            evaluator->setVariable(this->getLValueName(), literal->getValue());
        }

        return {};
    }

}