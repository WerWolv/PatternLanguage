#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeTernaryExpression : public ASTNode {
    public:
        ASTNodeTernaryExpression(std::unique_ptr<ASTNode> &&first, std::unique_ptr<ASTNode> &&second, std::unique_ptr<ASTNode> &&third, Token::Operator op)
            : ASTNode(), m_first(std::move(first)), m_second(std::move(second)), m_third(std::move(third)), m_operator(op) { }

        ASTNodeTernaryExpression(const ASTNodeTernaryExpression &other) : ASTNode(other) {
            this->m_operator = other.m_operator;
            this->m_first    = other.m_first->clone();
            this->m_second   = other.m_second->clone();
            this->m_third    = other.m_third->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTernaryExpression(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            if (this->getFirstOperand() == nullptr || this->getSecondOperand() == nullptr || this->getThirdOperand() == nullptr)
                err::E0002.throwError("Void expression used in ternary expression.", "If you used a function for one of the operands, make sure it returned a value.", this);

            auto firstNode  = this->getFirstOperand()->evaluate(evaluator);
            auto secondNode = this->getSecondOperand()->evaluate(evaluator);
            auto thirdNode  = this->getThirdOperand()->evaluate(evaluator);

            auto *first  = dynamic_cast<ASTNodeLiteral *>(firstNode.get());
            auto *second = dynamic_cast<ASTNodeLiteral *>(secondNode.get());
            auto *third  = dynamic_cast<ASTNodeLiteral *>(thirdNode.get());

            if (first == nullptr || second == nullptr || third == nullptr)
                err::E0010.throwError("Cannot use void expression in ternary expression.", {}, this);

            auto condition = std::visit(wolv::util::overloaded {
                [](const std::string &value) -> bool { return !value.empty(); },
                [this](const std::shared_ptr<ptrn::Pattern> &pattern) -> bool { err::E0002.throwError(fmt::format("Cannot cast {} to bool.", pattern->getTypeName()), {}, this); },
                [](auto &&value) -> bool { return bool(value); }
            }, first->getValue());

            return std::visit(wolv::util::overloaded {
                [condition]<typename T, typename U> requires std::convertible_to<T, U> && std::convertible_to<U, T>
                (const T &second, const U &third) -> std::unique_ptr<ASTNode> { return std::unique_ptr<ASTNode>(new ASTNodeLiteral(condition ? second : third)); },
                [this](auto &&, auto &&) -> std::unique_ptr<ASTNode> { err::E0002.throwError("Second and third operand in ternary expression have different types.", {}, this); }
            }, second->getValue(), third->getValue());
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getFirstOperand() const { return this->m_first; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getSecondOperand() const { return this->m_second; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getThirdOperand() const { return this->m_third; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

    private:
        std::unique_ptr<ASTNode> m_first, m_second, m_third;
        Token::Operator m_operator;
    };

}