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

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeTryCatchStatement::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto startOffset = evaluator->getReadOffset();
        auto &scope = evaluator->getScope(0);

        auto startScopeSize = scope.scope->size();
        auto startHeapSize = scope.heapStartSize;


        try {
            for (auto &node : this->m_tryBody) {
                auto newPatterns = node->createPatterns(evaluator);
                for (auto &pattern : newPatterns) {
                    scope.scope->push_back(std::move(pattern));
                }

                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;
            }
        } catch (err::EvaluatorError::Exception &error) {
            evaluator->setReadOffset(startOffset);

            scope.scope->resize(startScopeSize);
            scope.heapStartSize = startHeapSize;

            for (auto &node : this->m_catchBody) {
                auto newPatterns = node->createPatterns(evaluator);
                for (auto &pattern : newPatterns) {
                    scope.scope->push_back(std::move(pattern));
                }

                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;
            }
        }

        return {};
    }


    ASTNode::FunctionResult ASTNodeTryCatchStatement::execute(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto variables     = *evaluator->getScope(0).scope;
        auto parameterPack = evaluator->getScope(0).parameterPack;

        evaluator->pushScope(nullptr, variables);
        evaluator->getScope(0).parameterPack = parameterPack;
        ON_SCOPE_EXIT {
            evaluator->popScope();
        };

        try {
            for (auto &statement : this->m_tryBody) {
                auto result = statement->execute(evaluator);

                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    if (!result.has_value())
                        return std::nullopt;

                    return std::visit(wolv::util::overloaded {
                            [](const auto &value) -> FunctionResult {
                                return value;
                            },
                            [evaluator](const std::shared_ptr<ptrn::Pattern> &pattern) -> FunctionResult {
                                auto &prevScope = evaluator->getScope(-1);
                                auto &currScope = evaluator->getScope(0);

                                prevScope.heapStartSize = currScope.heapStartSize = evaluator->getHeap().size();

                                return pattern;
                            }
                    }, result.value());
                }
            }
        } catch (err::EvaluatorError::Exception &error) {
            for (auto &statement : this->m_catchBody) {
                auto result = statement->execute(evaluator);

                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    if (!result.has_value())
                        return std::nullopt;

                    return std::visit(wolv::util::overloaded {
                            [](const auto &value) -> FunctionResult {
                                return value;
                            },
                            [evaluator](const std::shared_ptr<ptrn::Pattern> &pattern) -> FunctionResult {
                                auto &prevScope = evaluator->getScope(-1);
                                auto &currScope = evaluator->getScope(0);

                                prevScope.heapStartSize = currScope.heapStartSize = evaluator->getHeap().size();

                                return pattern;
                            }
                    }, result.value());
                }
            }
        }

        return std::nullopt;
    }

}