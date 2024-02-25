#include <pl/core/ast/ast_node_while_statement.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeWhileStatement::ASTNodeWhileStatement(std::unique_ptr<ASTNode> &&condition, std::vector<std::unique_ptr<ASTNode>> &&body, std::unique_ptr<ASTNode> &&postExpression)
    : ASTNode(), m_condition(std::move(condition)), m_body(std::move(body)), m_postExpression(std::move(postExpression)) { }

    ASTNodeWhileStatement::ASTNodeWhileStatement(const ASTNodeWhileStatement &other) : ASTNode(other) {
        this->m_condition = other.m_condition->clone();

        for (auto &statement : other.m_body)
            this->m_body.push_back(statement->clone());

        if (other.m_postExpression != nullptr)
            this->m_postExpression = other.m_postExpression->clone();
        else
            this->m_postExpression = nullptr;
    }

    ASTNode::FunctionResult ASTNodeWhileStatement::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        u64 loopIterations = 0;
        while (evaluateCondition(evaluator)) {
            evaluator->handleAbort();

            auto variables         = *evaluator->getScope(0).scope;
            auto parameterPack     = evaluator->getScope(0).parameterPack;

            evaluator->pushScope(nullptr, variables);
            evaluator->getScope(0).parameterPack = parameterPack;
            ON_SCOPE_EXIT {
                              evaluator->popScope();
                          };

            auto ctrlFlow = ControlFlowStatement::None;
            for (auto &statement : this->getBody()) {
                auto result = statement->execute(evaluator);

                ctrlFlow = evaluator->getCurrentControlFlowStatement();
                if (ctrlFlow == ControlFlowStatement::Return)
                    return result;
                else if (ctrlFlow != ControlFlowStatement::None) {
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                    break;
                }
            }

            if (this->m_postExpression != nullptr)
                this->m_postExpression->execute(evaluator);

            loopIterations++;
            if (loopIterations >= evaluator->getLoopLimit())
                err::E0007.throwError(fmt::format("Loop iterations exceeded set limit of {}", evaluator->getLoopLimit()), "If this is intended, try increasing the limit using '#pragma loop_limit <new_limit>'.");

            evaluator->handleAbort();

            if (ctrlFlow == ControlFlowStatement::Break)
                break;
            else if (ctrlFlow == ControlFlowStatement::Continue)
                continue;
        }

        return std::nullopt;
    }

    [[nodiscard]] bool ASTNodeWhileStatement::evaluateCondition(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        const auto node    = this->getCondition()->evaluate(evaluator);
        const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot use void expression as condition.", {}, this);

        return std::visit(wolv::util::overloaded {
                [](const std::string &value) -> bool { return !value.empty(); },
                [this](ptrn::Pattern *const &pattern) -> bool { err::E0002.throwError(fmt::format("Cannot cast {} to bool.", pattern->getTypeName()), {}, this); },
                [](auto &&value) -> bool { return value != 0; }
        }, literal->getValue());
    }

}