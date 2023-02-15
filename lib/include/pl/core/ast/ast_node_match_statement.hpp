#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>

namespace pl::core::ast {
    class ASTNodeMatchStatement : public ASTNode {

        struct Case {
            std::vector<std::unique_ptr<ASTNode>> conditions;
            std::vector<std::unique_ptr<ASTNode>> bodies;
        };

    public:
        explicit ASTNodeMatchStatement(std::vector<Case> cases, std::vector<std::unique_ptr<ASTNode>> &&defaultCase) 
            : ASTNode(), m_cases(cases), m_defaultCase(std::move(defaultCase)) { }

        ASTNodeMatchStatement(const ASTNodeMatchStatement &other) : ASTNode(other) {

            for (auto &case_ : other.m_cases) {
                Case newCase;
                for (auto &condition : case_.conditions)
                    newCase.conditions.push_back(condition->clone());
                for (auto &body : case_.bodies)
                    newCase.bodies.push_back(body->clone());
                this->m_cases.push_back(newCase);
            }

            for (auto &defaultCase : other.m_defaultCase)
                this->m_defaultCase.push_back(defaultCase->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMatchStatement(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto &scope = *evaluator->getScope(0).scope;
            auto &body  = getCaseBody(evaluator);

            for (auto &node : body) {
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

            auto &body = getCaseBody(evaluator);

            auto variables     = *evaluator->getScope(0).scope;
            auto parameterPack = evaluator->getScope(0).parameterPack;

            evaluator->pushScope(nullptr, variables);
            evaluator->getScope(0).parameterPack = parameterPack;
            PL_ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            for (auto &statement : body) {
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
    // compare two vectors of ASTNodes
        [[nodiscard]] bool evalulate(const std::vector<std::unique_ptr<ASTNode>> &conditions, Evaluator* evaluator) const {
            for (auto &condition : conditions) {
                if(!evaluateCondition(condition, evaluator))
                    return false;
            }
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> getCaseBody(Evaluator *evaluator) const {
            for(auto &case_ : m_cases) {
                if(evalulate(case_.conditions, evaluator))
                    return case_.bodies;
            }
            return m_defaultCase;
        }

        std::vector<Case> m_cases;
        std::vector<std::unique_ptr<ASTNode>> m_defaultCase;
    };
}