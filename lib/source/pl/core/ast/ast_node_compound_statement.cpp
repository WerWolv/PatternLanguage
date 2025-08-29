#include <pl/core/ast/ast_node_compound_statement.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeCompoundStatement::ASTNodeCompoundStatement(std::vector<std::unique_ptr<ASTNode>> &&statements, bool newScope) :  m_newScope(newScope) {
        for (auto &statement : statements) {
            this->m_statements.emplace_back(std::move(statement));
        }
    }

    ASTNodeCompoundStatement::ASTNodeCompoundStatement(std::vector<std::shared_ptr<ASTNode>> &&statements, bool newScope) : m_statements(statements), m_newScope(newScope) {
    }

    ASTNodeCompoundStatement::ASTNodeCompoundStatement(const ASTNodeCompoundStatement &other) : ASTNode(other), Attributable(other) {
        for (const auto &statement : other.m_statements) {
            this->m_statements.push_back(statement->clone());
        }

        this->m_newScope = other.m_newScope;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeCompoundStatement::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::unique_ptr<ASTNode> result = nullptr;

        for (const auto &statement : this->m_statements) {
            result = statement->evaluate(evaluator);
        }

        return result;
    }

    void ASTNodeCompoundStatement::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        for (const auto &statement : this->m_statements) {
            std::vector<std::shared_ptr<ptrn::Pattern>> newPatterns;
            statement->createPatterns(evaluator, newPatterns);
            std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(resultPatterns));
        }
    }

    ASTNode::FunctionResult ASTNodeCompoundStatement::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        FunctionResult result;
        std::vector<std::shared_ptr<ptrn::Pattern>> variables;

        auto scopeGuard = SCOPE_GUARD {
            evaluator->popScope();
        };

        if (this->m_newScope) {
            evaluator->pushScope(nullptr, variables, false);
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