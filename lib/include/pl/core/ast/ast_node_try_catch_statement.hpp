#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    class ASTNodeTryCatchStatement : public ASTNode {
    public:
        explicit ASTNodeTryCatchStatement(std::vector<std::unique_ptr<ASTNode>> &&tryBody, std::vector<std::unique_ptr<ASTNode>> &&catchBody)
            : ASTNode(), m_tryBody(std::move(tryBody)), m_catchBody(std::move(catchBody)) { }


        ASTNodeTryCatchStatement(const ASTNodeTryCatchStatement &other) : ASTNode(other) {
            for (auto &statement : other.m_tryBody)
                this->m_tryBody.push_back(statement->clone());
            for (auto &statement : other.m_catchBody)
                this->m_catchBody.push_back(statement->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTryCatchStatement(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto startOffset = evaluator->getReadOffset();
            auto &scope = evaluator->getScope(0);

            auto startScopeSize = scope.scope->size();
            auto startHeapSize = scope.heapStartSize;
            auto startSavedPatternSize = scope.savedPatterns.size();


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
                scope.savedPatterns.resize(startSavedPatternSize);

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


        FunctionResult execute(Evaluator *evaluator) const override {
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
                                [evaluator](ptrn::Pattern *pattern) -> FunctionResult {
                                    auto clonedPattern = pattern->clone();
                                    auto result = clonedPattern.get();

                                    auto &prevScope = evaluator->getScope(-1);
                                    auto &currScope = evaluator->getScope(0);

                                    prevScope.savedPatterns.push_back(std::move(clonedPattern));
                                    prevScope.heapStartSize = currScope.heapStartSize = evaluator->getHeap().size();

                                    return result;
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
                                [evaluator](ptrn::Pattern *pattern) -> FunctionResult {
                                    auto clonedPattern = pattern->clone();
                                    auto result = clonedPattern.get();

                                    auto &prevScope = evaluator->getScope(-1);
                                    auto &currScope = evaluator->getScope(0);

                                    prevScope.savedPatterns.push_back(std::move(clonedPattern));
                                    prevScope.heapStartSize = currScope.heapStartSize = evaluator->getHeap().size();

                                    return result;
                                }
                        }, result.value());
                    }
                }
            }

            return std::nullopt;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getTryBody() const {
            return this->m_tryBody;
        }
        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getCatchBody() const {
            return this->m_catchBody;
        }

    private:

        std::vector<std::unique_ptr<ASTNode>> m_tryBody, m_catchBody;
    };

}