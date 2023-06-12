#include <pl/core/ast/ast_node_match_statement.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>

namespace pl::core::ast {

    ASTNodeMatchStatement::ASTNodeMatchStatement(std::vector<MatchCase> cases, std::optional<MatchCase> defaultCase)
    : ASTNode(), m_cases(std::move(cases)), m_defaultCase(std::move(defaultCase)) { }

    ASTNodeMatchStatement::ASTNodeMatchStatement(const ASTNodeMatchStatement &other) : ASTNode(other) {
        for (auto &matchCase : other.m_cases)
            this->m_cases.push_back(matchCase);
        if(other.m_defaultCase) {
            this->m_defaultCase.emplace(*other.m_defaultCase);
        } else {
            this->m_defaultCase = std::nullopt;
        }
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeMatchStatement::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto &scope = *evaluator->getScope(0).scope;
        auto body   = this->getCaseBody(evaluator);

        if (body == nullptr) return { };

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

    ASTNode::FunctionResult ASTNodeMatchStatement::execute(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto body = this->getCaseBody(evaluator);
        if (body == nullptr) return std::nullopt;

        auto &currScope = evaluator->getScope(0);

        auto variables     = *currScope.scope;
        auto parameterPack = currScope.parameterPack;

        evaluator->pushScope(nullptr, variables);
        evaluator->getScope(0).parameterPack = parameterPack;
        ON_SCOPE_EXIT {
            evaluator->popScope();
        };

        for (auto &statement : *body) {
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

        return std::nullopt;
    }

    [[nodiscard]] bool ASTNodeMatchStatement::evaluateCondition(const std::unique_ptr<ASTNode> &condition, Evaluator *evaluator) const {
        const auto node    = condition->evaluate(evaluator);
        const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());

        if (literal == nullptr)
            err::E0010.throwError("Cannot use void expression as condition.", {}, this);

        return std::visit(wolv::util::overloaded {
                [](const std::string &value) -> bool { return !value.empty(); },
                [this](ptrn::Pattern *const &pattern) -> bool { err::E0004.throwError(fmt::format("Cannot cast value of type '{}' to type 'bool'.", pattern->getTypeName()), {}, this); },
                [](auto &&value) -> bool { return value != 0; }
        }, literal->getValue());
    }

    [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>* ASTNodeMatchStatement::getCaseBody(Evaluator *evaluator) const {
        std::optional<size_t> matchedBody;
        for (size_t i = 0; i < this->m_cases.size(); i++) {
            auto &condition = this->m_cases[i].condition;
            if (evaluateCondition(condition, evaluator)) {
                if(matchedBody.has_value())
                    err::E0013.throwError(fmt::format("Match is ambiguous. Both case {} and {} match.", matchedBody.value() + 1, i + 1), {}, condition.get());
                matchedBody = i;
            }
        }

        if (matchedBody.has_value())
            return &this->m_cases[matchedBody.value()].body;

        // if no found, then attempt to use default case
        if (this->m_defaultCase.has_value())
            return &this->m_defaultCase.value().body;

        return nullptr;
    }

}