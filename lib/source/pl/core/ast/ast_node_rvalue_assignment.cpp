#include <pl/core/ast/ast_node_rvalue_assignment.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeRValueAssignment::ASTNodeRValueAssignment(std::unique_ptr<ASTNode> &&lvalue, std::unique_ptr<ASTNode> &&rvalue)
        : m_lvalue(std::move(lvalue)), m_rvalue(std::move(rvalue)) { }

    ASTNodeRValueAssignment::ASTNodeRValueAssignment(const ASTNodeRValueAssignment &other) : ASTNode(other) {
        this->m_lvalue = other.m_lvalue->clone();
        this->m_rvalue = other.m_rvalue->clone();
    }


    void ASTNodeRValueAssignment::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &) const {
        this->execute(evaluator);
    }

    ASTNode::FunctionResult ASTNodeRValueAssignment::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::vector<std::shared_ptr<ptrn::Pattern>> lhs;
        this->getLValue()->createPatterns(evaluator, lhs);
        auto rhs = this->getRValue()->evaluate(evaluator);

        if (lhs.empty())
            err::E0003.throwError("Cannot find variable in this scope.", {}, this->getLocation());

        auto &pattern = lhs.front();
        const auto literal = dynamic_cast<ASTNodeLiteral *>(rhs.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot assign void expression to variable.", {}, this->getLocation());

        evaluator->setVariable(pattern, literal->getValue());

        return {};
    }

}