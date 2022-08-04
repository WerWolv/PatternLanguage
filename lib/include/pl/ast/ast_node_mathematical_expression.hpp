#pragma once

#include <pl/ast/ast_node.hpp>
#include "helpers/concepts.hpp"

#include <pl/patterns/pattern_enum.hpp>

namespace pl {

#define FLOAT_BIT_OPERATION(name)                                                       \
    auto name(pl::floating_point auto left, auto right) const {                         \
        pl::unused(left, right);                                                        \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this);                                                                      \
        return 0;                                                                       \
    }                                                                                   \
    auto name(auto left, pl::floating_point auto right) const {                         \
        pl::unused(left, right);                                                        \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this);                                                                      \
        return 0;                                                                       \
    }                                                                                   \
    auto name(pl::floating_point auto left, pl::floating_point auto right) const {      \
        pl::unused(left, right);                                                        \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this);                                                                      \
        return 0;                                                                       \
    }                                                                                   \
    auto name(pl::integral auto left, pl::integral auto right) const

    class ASTNodeMathematicalExpression : public ASTNode {

        FLOAT_BIT_OPERATION(shiftLeft) {
            return left << right;
        }

        FLOAT_BIT_OPERATION(shiftRight) {
            return left >> right;
        }

        FLOAT_BIT_OPERATION(bitAnd) {
            return left & right;
        }

        FLOAT_BIT_OPERATION(bitOr) {
            return left | right;
        }

        FLOAT_BIT_OPERATION(bitXor) {
            return left ^ right;
        }

        FLOAT_BIT_OPERATION(bitNot) {
            pl::unused(left);

            return ~static_cast<u128>(right);
        }

        FLOAT_BIT_OPERATION(modulus) {
            return left % right;
        }

#undef FLOAT_BIT_OPERATION
    public:
        ASTNodeMathematicalExpression(std::unique_ptr<ASTNode> &&left, std::unique_ptr<ASTNode> &&right, Token::Operator op)
            : ASTNode(), m_left(std::move(left)), m_right(std::move(right)), m_operator(op) { }

        ASTNodeMathematicalExpression(const ASTNodeMathematicalExpression &other) : ASTNode(other) {
            this->m_operator = other.m_operator;
            this->m_left     = other.m_left->clone();
            this->m_right    = other.m_right->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMathematicalExpression(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            if (this->getLeftOperand() == nullptr || this->getRightOperand() == nullptr)
                err::E0002.throwError("Void expression used in ternary expression.", "If you used a function for one of the operands, make sure it returned a value.", this);

            auto leftNode  = this->getLeftOperand()->evaluate(evaluator);
            auto rightNode = this->getRightOperand()->evaluate(evaluator);

            auto *left  = dynamic_cast<ASTNodeLiteral *>(leftNode.get());
            auto *right = dynamic_cast<ASTNodeLiteral *>(rightNode.get());

            constexpr static auto Decay = [](const Token::Literal &literal) {
                return std::visit(overloaded {
                        [](Pattern *pattern) -> Token::Literal {
                            if (auto enumPattern = dynamic_cast<PatternEnum*>(pattern))
                                return u128(enumPattern->getValue());
                            else
                                return pattern;
                        },
                        [](auto &&value) -> Token::Literal  { return value; }
                }, literal);
            };

            const auto leftValue = Decay(left->getValue());
            const auto rightValue = Decay(right->getValue());

            const static auto throwInvalidOperandError = [this] [[noreturn]]{
                err::E0002.throwError("Invalid operand used in mathematical expression.", { }, this);
            };

            return std::unique_ptr<ASTNode>(std::visit(overloaded {
               [](u128, Pattern *const ) -> ASTNode * { throwInvalidOperandError(); },
               [](i128, Pattern *const &) -> ASTNode * { throwInvalidOperandError(); },
               [](double, Pattern *const &) -> ASTNode * { throwInvalidOperandError(); },
               [](char, Pattern *const &) -> ASTNode * { throwInvalidOperandError(); },
               [](bool, Pattern *const &) -> ASTNode * { throwInvalidOperandError(); },
               [](const std::string &, Pattern *const &) -> ASTNode * { throwInvalidOperandError(); },
               [](Pattern *const &, u128) -> ASTNode * { throwInvalidOperandError(); },
               [](Pattern *const &, i128) -> ASTNode * { throwInvalidOperandError();},
               [](Pattern *const &, double) -> ASTNode * { throwInvalidOperandError(); },
               [](Pattern *const &, char) -> ASTNode * { throwInvalidOperandError(); },
               [](Pattern *const &, bool) -> ASTNode * { throwInvalidOperandError(); },
               [](Pattern *const &, const std::string &) -> ASTNode * { throwInvalidOperandError(); },
               [](Pattern *const &, Pattern *const &) -> ASTNode * { throwInvalidOperandError(); },

               [](auto &&, const std::string &) -> ASTNode * { throwInvalidOperandError(); },
               [this](const std::string &left, auto &&right) -> ASTNode * {
                   switch (this->getOperator()) {
                       case Token::Operator::Star:
                           {
                               if (static_cast<i128>(right) < 0)
                                   err::E0002.throwError("Cannot repeat string a negative number of times.", { }, this);

                               std::string result;
                               for (u128 i = 0; i < static_cast<u128>(right); i++)
                                   result += left;
                               return new ASTNodeLiteral(result);
                           }
                       default:
                           throwInvalidOperandError();
                   }
               },
               [this](const std::string &left, const std::string &right) -> ASTNode * {
                   switch (this->getOperator()) {
                       case Token::Operator::Plus:
                           return new ASTNodeLiteral(left + right);
                       case Token::Operator::BoolEqual:
                           return new ASTNodeLiteral(left == right);
                       case Token::Operator::BoolNotEqual:
                           return new ASTNodeLiteral(left != right);
                       case Token::Operator::BoolGreaterThan:
                           return new ASTNodeLiteral(left > right);
                       case Token::Operator::BoolLessThan:
                           return new ASTNodeLiteral(left < right);
                       case Token::Operator::BoolGreaterThanOrEqual:
                           return new ASTNodeLiteral(left >= right);
                       case Token::Operator::BoolLessThanOrEqual:
                           return new ASTNodeLiteral(left <= right);
                       default:
                           throwInvalidOperandError();
                   }
               },
               [this](const std::string &left, char right) -> ASTNode * {
                   switch (this->getOperator()) {
                       case Token::Operator::Plus:
                           return new ASTNodeLiteral(left + right);
                       default:
                           throwInvalidOperandError();
                   }
               },
               [this](char left, const std::string &right) -> ASTNode * {
                   switch (this->getOperator()) {
                       case Token::Operator::Plus:
                           return new ASTNodeLiteral(left + right);
                       default:
                           throwInvalidOperandError();
                   }
               },
               [this](auto &&left, auto &&right) -> ASTNode * {
                   switch (this->getOperator()) {
                       case Token::Operator::Plus:
                           return new ASTNodeLiteral(left + right);
                       case Token::Operator::Minus:
                           return new ASTNodeLiteral(left - right);
                       case Token::Operator::Star:
                           return new ASTNodeLiteral(left * right);
                       case Token::Operator::Slash:
                           if (right == 0) err::E0002.throwError("Division by zero.", { }, this);
                           return new ASTNodeLiteral(left / right);
                       case Token::Operator::Percent:
                           if (right == 0) err::E0002.throwError("Division by zero.", { }, this);
                           return new ASTNodeLiteral(modulus(left, right));
                       case Token::Operator::LeftShift:
                           return new ASTNodeLiteral(shiftLeft(left, right));
                       case Token::Operator::RightShift:
                           return new ASTNodeLiteral(shiftRight(left, right));
                       case Token::Operator::BitAnd:
                           return new ASTNodeLiteral(bitAnd(left, right));
                       case Token::Operator::BitXor:
                           return new ASTNodeLiteral(bitXor(left, right));
                       case Token::Operator::BitOr:
                           return new ASTNodeLiteral(bitOr(left, right));
                       case Token::Operator::BitNot:
                           return new ASTNodeLiteral(bitNot(left, right));
                       case Token::Operator::BoolEqual:
                           return new ASTNodeLiteral(bool(left == static_cast<decltype(left)>(right)));
                       case Token::Operator::BoolNotEqual:
                           return new ASTNodeLiteral(bool(left != static_cast<decltype(left)>(right)));
                       case Token::Operator::BoolGreaterThan:
                           return new ASTNodeLiteral(bool(left > static_cast<decltype(left)>(right)));
                       case Token::Operator::BoolLessThan:
                           return new ASTNodeLiteral(bool(left < static_cast<decltype(left)>(right)));
                       case Token::Operator::BoolGreaterThanOrEqual:
                           return new ASTNodeLiteral(bool(left >= static_cast<decltype(left)>(right)));
                       case Token::Operator::BoolLessThanOrEqual:
                           return new ASTNodeLiteral(bool(left <= static_cast<decltype(left)>(right)));
                       case Token::Operator::BoolAnd:
                           return new ASTNodeLiteral(bool(left && right));
                       case Token::Operator::BoolXor:
                           return new ASTNodeLiteral(bool((left && !right) || (!left && right)));
                       case Token::Operator::BoolOr:
                           return new ASTNodeLiteral(bool(left || right));
                       case Token::Operator::BoolNot:
                           return new ASTNodeLiteral(bool(!right));
                       default:
                           throwInvalidOperandError();
                   }
               }
            },
            leftValue,
            rightValue));
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getLeftOperand() const { return this->m_left; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getRightOperand() const { return this->m_right; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

    private:
        std::unique_ptr<ASTNode> m_left, m_right;
        Token::Operator m_operator;
    };

#undef FLOAT_BIT_OPERATION

}