#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeControlFlowStatement : public ASTNode {
    public:
        explicit ASTNodeControlFlowStatement(ControlFlowStatement type, std::unique_ptr<ASTNode> &&rvalue) : m_type(type), m_rvalue(std::move(rvalue)) {
        }

        ASTNodeControlFlowStatement(const ASTNodeControlFlowStatement &other) : ASTNode(other) {
            this->m_type = other.m_type;

            if (other.m_rvalue != nullptr)
                this->m_rvalue = other.m_rvalue->clone();
            else
                this->m_rvalue = nullptr;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeControlFlowStatement(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            if (auto result = this->execute(evaluator); result.has_value())
                evaluator->setMainResult(*result);

            return { };
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

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
                        [evaluator](ptrn::Pattern *pattern) -> FunctionResult {
                            auto clonedPattern = pattern->clone();
                            auto result = clonedPattern.get();

                            auto &prevScope = evaluator->getScope(-1);
                            auto &currScope = evaluator->getScope(0);

                            prevScope.savedPatterns.push_back(std::move(clonedPattern));
                            currScope.heapStartSize = evaluator->getHeap().size();

                            return result;
                        }
                    }, literal->getValue());
                }
            }
        }

    private:
        ControlFlowStatement m_type;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}