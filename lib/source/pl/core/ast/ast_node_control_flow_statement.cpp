#include <pl/core/ast/ast_node_control_flow_statement.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeControlFlowStatement::ASTNodeControlFlowStatement(ControlFlowStatement type, std::unique_ptr<ASTNode> &&rvalue)
        : m_type(type), m_rvalue(std::move(rvalue)) { }

    ASTNodeControlFlowStatement::ASTNodeControlFlowStatement(const ASTNodeControlFlowStatement &other) : ASTNode(other) {
        this->m_type = other.m_type;

        if (other.m_rvalue != nullptr)
            this->m_rvalue = other.m_rvalue->clone();
        else
            this->m_rvalue = nullptr;
    }

    void ASTNodeControlFlowStatement::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        if (auto result = this->execute(evaluator); result.has_value())
            evaluator->setMainResult(*result);
    }

    ASTNode::FunctionResult ASTNodeControlFlowStatement::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        if (this->m_rvalue == nullptr) {
            evaluator->setCurrentControlFlowStatement(this->m_type);
            return std::nullopt;
        } else {
            auto returnValue = this->m_rvalue->evaluate(evaluator);
            auto literal     = dynamic_cast<ASTNodeLiteral *>(returnValue.get());

            evaluator->setCurrentControlFlowStatement(this->m_type);

            if (literal == nullptr)
                return std::nullopt;
            else {
                return std::visit(wolv::util::overloaded {
                        [](const auto &value) -> FunctionResult {
                            return value;
                        },
                        [evaluator](const std::shared_ptr<ptrn::Pattern> &pattern) -> FunctionResult {
                            auto &currScope = evaluator->getScope(0);

                            currScope.heapStartSize = evaluator->getHeap().size();

                            return pattern;
                        }
                }, literal->getValue());
            }
        }
    }

}