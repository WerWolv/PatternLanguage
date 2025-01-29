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

    void ASTNodeLValueAssignment::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &) const {
        this->execute(evaluator);
    }

    ASTNode::FunctionResult ASTNodeLValueAssignment::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        const auto node    = this->getRValue()->evaluate(evaluator);
        const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot assign void expression to variable.", {}, this->getLocation());

        auto value = literal->getValue();
        if (this->getLValueName() == "$")
            evaluator->setReadOffset(u64(value.toUnsigned()));
        else {
            auto variable = evaluator->getVariableByName(this->getLValueName());
            applyVariableAttributes(evaluator, this, variable);

            if (value.isPattern()) {
                auto decayedValue = value.toPattern()->getValue();
                if (!decayedValue.isPattern())
                    value = std::move(decayedValue);
            }

            evaluator->setVariable(this->getLValueName(), value);
        }

        return {};
    }

}