#include <pl/core/ast/ast_node_conditional_statement.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeConditionalStatement::ASTNodeConditionalStatement(std::unique_ptr<ASTNode> condition, std::vector<std::unique_ptr<ASTNode>> &&trueBody, std::vector<std::unique_ptr<ASTNode>> &&falseBody)
    : ASTNode(), m_condition(std::move(condition)), m_trueBody(std::move(trueBody)), m_falseBody(std::move(falseBody)) { }


    ASTNodeConditionalStatement::ASTNodeConditionalStatement(const ASTNodeConditionalStatement &other) : ASTNode(other) {
        this->m_condition = other.m_condition->clone();

        for (auto &statement : other.m_trueBody)
            this->m_trueBody.push_back(statement->clone());
        for (auto &statement : other.m_falseBody)
            this->m_falseBody.push_back(statement->clone());
    }

    void ASTNodeConditionalStatement::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto &scope = *evaluator->getScope(0).scope;
        auto &body  = evaluateCondition(getCondition(), evaluator) ? this->m_trueBody : this->m_falseBody;

        for (auto &node : body) {
            std::vector<std::shared_ptr<ptrn::Pattern>> newPatterns;
            node->createPatterns(evaluator, newPatterns);
            for (auto &pattern : newPatterns) {
                pattern->setSection(evaluator->getSectionId());
                scope.push_back(std::move(pattern));
            }

            if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                break;
        }
    }

    ASTNode::FunctionResult ASTNodeConditionalStatement::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto &body = evaluateCondition(getCondition(), evaluator) ? this->m_trueBody : this->m_falseBody;

        std::vector<std::shared_ptr<ptrn::Pattern>> variables;
        auto parameterPack = evaluator->getScope(0).parameterPack;

        evaluator->pushScope(nullptr, variables, false);
        evaluator->getScope(0).parameterPack = parameterPack;
        ON_SCOPE_EXIT {
            evaluator->popScope();
        };

        for (auto &statement : body) {
            auto result = statement->execute(evaluator);

            if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                return result;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] bool ASTNodeConditionalStatement::evaluateCondition(const std::unique_ptr<ASTNode> &condition, Evaluator *evaluator) const {
        const auto node    = condition->evaluate(evaluator);
        const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot use void expression as condition.", {}, this->getLocation());

        return std::visit(wolv::util::overloaded {
                [](const std::string &value) -> bool { return !value.empty(); },
                [this](ptrn::Pattern *const &pattern) -> bool { err::E0004.throwError(fmt::format("Cannot cast value of type '{}' to type 'bool'.", pattern->getTypeName()), {}, this->getLocation()); },
                [](auto &&value) -> bool { return value != 0; }
        }, literal->getValue());
    }

}