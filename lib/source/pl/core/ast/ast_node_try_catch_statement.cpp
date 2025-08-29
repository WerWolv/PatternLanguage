#include <pl/core/ast/ast_node_try_catch_statement.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeTryCatchStatement::ASTNodeTryCatchStatement(std::vector<std::unique_ptr<ASTNode>> &&tryBody, std::vector<std::unique_ptr<ASTNode>> &&catchBody)
        : m_tryBody(std::move(tryBody)), m_catchBody(std::move(catchBody)) { }


    ASTNodeTryCatchStatement::ASTNodeTryCatchStatement(const ASTNodeTryCatchStatement &other) : ASTNode(other) {
        for (auto &statement : other.m_tryBody)
            this->m_tryBody.push_back(statement->clone());
        for (auto &statement : other.m_catchBody)
            this->m_catchBody.push_back(statement->clone());
    }

    void ASTNodeTryCatchStatement::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getReadOffset();
        auto &scope = evaluator->getScope(0);

        auto startScopeSize = scope.scope->size();


        try {
            for (auto &node : this->m_tryBody) {
                std::vector<std::shared_ptr<ptrn::Pattern>> newPatterns;
                node->createPatterns(evaluator, newPatterns);
                for (auto &pattern : newPatterns) {
                    pattern->setSection(evaluator->getSectionId());
                    scope.scope->push_back(std::move(pattern));
                }

                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;
            }
        } catch (err::EvaluatorError::Exception &) {
            evaluator->setReadOffset(startOffset);

            scope.scope->resize(startScopeSize);

            for (auto &node : this->m_catchBody) {
                std::vector<std::shared_ptr<ptrn::Pattern>> newPatterns;
                node->createPatterns(evaluator, newPatterns);
                for (auto &pattern : newPatterns) {
                    pattern->setSection(evaluator->getSectionId());
                    scope.scope->push_back(std::move(pattern));
                }

                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;
            }
        }
    }


    ASTNode::FunctionResult ASTNodeTryCatchStatement::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::vector<std::shared_ptr<ptrn::Pattern>> variables;
        auto parameterPack = evaluator->getScope(0).parameterPack;

        evaluator->pushScope(nullptr, variables, false);
        evaluator->getScope(0).parameterPack = parameterPack;
        ON_SCOPE_EXIT {
            evaluator->popScope();
        };

        try {
            for (auto &statement : this->m_tryBody) {
                auto result = statement->execute(evaluator);

                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    return result;
                }
            }
        } catch (err::EvaluatorError::Exception &error) {
            for (auto &statement : this->m_catchBody) {
                auto result = statement->execute(evaluator);

                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

}