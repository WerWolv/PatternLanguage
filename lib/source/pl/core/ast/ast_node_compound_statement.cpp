#include <pl/core/ast/ast_node_compound_statement.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeCompoundStatement::ASTNodeCompoundStatement(std::vector<std::unique_ptr<ASTNode>> &&statements, bool newScope) : m_statements(std::move(statements)), m_newScope(newScope) {
    }

    ASTNodeCompoundStatement::ASTNodeCompoundStatement(const ASTNodeCompoundStatement &other) : ASTNode(other), Attributable(other) {
        for (const auto &statement : other.m_statements) {
            this->m_statements.push_back(statement->clone());
        }

        this->m_newScope = other.m_newScope;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeCompoundStatement::evaluate(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        std::unique_ptr<ASTNode> result = nullptr;

        for (const auto &statement : this->m_statements) {
            result = statement->evaluate(evaluator);
        }

        return result;
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeCompoundStatement::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        std::vector<std::shared_ptr<ptrn::Pattern>> result;

        for (const auto &statement : this->m_statements) {
            auto patterns = statement->createPatterns(evaluator);
            std::move(patterns.begin(), patterns.end(), std::back_inserter(result));
        }

        return result;
    }

    ASTNode::FunctionResult ASTNodeCompoundStatement::execute(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        FunctionResult result;
        auto variables         = *evaluator->getScope(0).scope;

        auto scopeGuard = SCOPE_GUARD {
            evaluator->popScope();
        };

        if (this->m_newScope) {
            evaluator->pushScope(nullptr, variables);
        } else {
            scopeGuard.release();
        }

        for (const auto &statement : this->m_statements) {
            result = statement->execute(evaluator);
            if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                break;
        }

        return result;
    }

    void ASTNodeCompoundStatement::addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) {
        for (const auto &statement : this->m_statements) {
            if (auto attributable = dynamic_cast<Attributable*>(statement.get())) {
                auto newAttribute = attribute->clone();
                attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(static_cast<ASTNodeAttribute*>(newAttribute.release())));
            }
        }
    }

}