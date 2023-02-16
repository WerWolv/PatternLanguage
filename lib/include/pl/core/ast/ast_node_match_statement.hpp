#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>
#include <utility>

namespace pl::core::ast {

    struct MatchCase {
        std::unique_ptr<ASTNode> condition;
        std::vector<std::unique_ptr<ASTNode>> body;

        MatchCase() = default;

        MatchCase(std::unique_ptr<ASTNode> condition, std::vector<std::unique_ptr<ASTNode>> body)
            : condition(std::move(condition)), body(std::move(body)) { }

        MatchCase(const MatchCase &other) {
            this->condition = other.condition->clone();
            for (auto &statement : other.body)
                this->body.push_back(statement->clone());
        }

        MatchCase(MatchCase &&other) noexcept {
            this->condition = std::move(other.condition);
            this->body = std::move(other.body);
        }
    };

    class ASTNodeMatchStatement : public ASTNode {

    public:
        explicit ASTNodeMatchStatement(std::vector<MatchCase> cases)
            : ASTNode(), m_cases(std::move(cases)) { }

        ASTNodeMatchStatement(const ASTNodeMatchStatement &other) : ASTNode(other) {
            for (auto &matchCase : other.m_cases)
                this->m_cases.push_back(matchCase);
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMatchStatement(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto &scope = *evaluator->getScope(0).scope;
            auto body  = getCaseBody(evaluator);

            if(!body) return {};

            for (auto &node : *body) {
                auto newPatterns = node->createPatterns(evaluator);
                for (auto &pattern : newPatterns) {
                    scope.push_back(std::move(pattern));
                }

                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;
            }

            return {};
        }

         FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto body = getCaseBody(evaluator);

            if(!body) return std::nullopt;

            auto variables     = *evaluator->getScope(0).scope;
            auto parameterPack = evaluator->getScope(0).parameterPack;

            evaluator->pushScope(nullptr, variables);
            evaluator->getScope(0).parameterPack = parameterPack;
            PL_ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            for (auto &statement : *body) {
                auto result = statement->execute(evaluator);

                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    if (!result.has_value())
                        return std::nullopt;

                    return std::visit(hlp::overloaded {
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

            return std::nullopt;
        }

    private:
        [[nodiscard]] bool evaluateCondition(const std::unique_ptr<ASTNode> &condition, Evaluator *evaluator) const {
            const auto node    = condition->evaluate(evaluator);
            const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (literal == nullptr)
                err::E0010.throwError("Cannot use void expression as condition.", {}, this);

            return std::visit(hlp::overloaded {
                [](const std::string &value) -> bool { return !value.empty(); },
                [this](ptrn::Pattern *const &pattern) -> bool { err::E0004.throwError(fmt::format("Cannot cast value of type '{}' to type 'bool'.", pattern->getTypeName()), {}, this); },
                [](auto &&value) -> bool { return value != 0; }
            }, literal->getValue());
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>* getCaseBody(Evaluator *evaluator) const {
            for(auto &case_ : m_cases) {
                if(evaluateCondition(case_.condition, evaluator))
                    return &case_.body;
            }
            // if no found, then return empty vector
            return nullptr;
        }

        std::vector<MatchCase> m_cases;
    };
}