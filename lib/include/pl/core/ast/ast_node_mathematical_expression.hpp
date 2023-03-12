#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/helpers/concepts.hpp>

#include <pl/patterns/pattern_enum.hpp>

namespace pl::core::ast {

#define FLOAT_BIT_OPERATION(name)                                                       \
    auto name(pl::floating_point auto left, auto right) const {                         \
        wolv::util::unused(left, right);                                                \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this);                                                                      \
        return 0;                                                                       \
    }                                                                                   \
    auto name(auto left, pl::floating_point auto right) const {                         \
        wolv::util::unused(left, right);                                                \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this);                                                                      \
        return 0;                                                                       \
    }                                                                                   \
    auto name(pl::floating_point auto left, pl::floating_point auto right) const {      \
        wolv::util::unused(left, right);                                                \
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
            wolv::util::unused(left);

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
            evaluator->updateRuntime(this);

            if (this->getLeftOperand() == nullptr || this->getRightOperand() == nullptr)
                err::E0002.throwError("Void expression used in ternary expression.", "If you used a function for one of the operands, make sure it returned a value.", this);

            const auto throwInvalidOperandError = [this] [[noreturn]]{
                err::E0002.throwError("Invalid operand used in mathematical expression.", { }, this);
            };

            auto leftNode  = this->getLeftOperand()->evaluate(evaluator);
            auto rightNode = this->getRightOperand()->evaluate(evaluator);

            auto leftLiteral  = dynamic_cast<ASTNodeLiteral *>(leftNode.get());
            auto rightLiteral = dynamic_cast<ASTNodeLiteral *>(rightNode.get());

            if (leftLiteral == nullptr || rightLiteral == nullptr)
                throwInvalidOperandError();

            auto leftValue = leftLiteral->getValue();
            auto rightValue = rightLiteral->getValue();

            auto handlePatternOperations = [&, this](auto left, auto right) {
                switch (this->getOperator()) {
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
            };

            return std::unique_ptr<ASTNode>(std::visit(wolv::util::overloaded {
                [&](u128 left, ptrn::Pattern *const &right)                 -> ASTNode * { return handlePatternOperations(left, right->getValue().toUnsigned());        },
                [&](i128 left, ptrn::Pattern *const &right)                 -> ASTNode * { return handlePatternOperations(left, right->getValue().toSigned());          },
                [&](double left, ptrn::Pattern *const &right)               -> ASTNode * { return handlePatternOperations(left, right->getValue().toFloatingPoint());   },
                [&](char left, ptrn::Pattern *const &right)                 -> ASTNode * { return handlePatternOperations(left, right->getValue().toSigned());          },
                [&](bool left, ptrn::Pattern *const &right)                 -> ASTNode * { return handlePatternOperations(left, right->getValue().toBoolean());         },
                [&](const std::string &left, ptrn::Pattern *const &right)   -> ASTNode * { return handlePatternOperations(left, right->getValue().toString(true));      },
                [&](ptrn::Pattern *const &left, u128 right)                 -> ASTNode * { return handlePatternOperations(left->getValue().toUnsigned(), right);        },
                [&](ptrn::Pattern *const &left, i128 right)                 -> ASTNode * { return handlePatternOperations(left->getValue().toSigned(), right);          },
                [&](ptrn::Pattern *const &left, double right)               -> ASTNode * { return handlePatternOperations(left->getValue().toFloatingPoint(), right);   },
                [&](ptrn::Pattern *const &left, char right)                 -> ASTNode * { return handlePatternOperations(left->getValue().toSigned(), right);          },
                [&](ptrn::Pattern *const &left, bool right)                 -> ASTNode * { return handlePatternOperations(left->getValue().toBoolean(), right);         },
                [&](ptrn::Pattern *const &left, const std::string &right)   -> ASTNode * { return handlePatternOperations(left->getValue().toString(true), right);      },
                [&](u128, const std::string &)                              -> ASTNode * { throwInvalidOperandError(); },
                [&](i128, const std::string &)                              -> ASTNode * { throwInvalidOperandError(); },
                [&](double, const std::string &)                            -> ASTNode * { throwInvalidOperandError(); },
                [&](bool, const std::string &)                              -> ASTNode * { throwInvalidOperandError(); },
                [&, this](ptrn::Pattern *const &left, ptrn::Pattern *const &right) -> ASTNode * {
                    std::vector<u8> leftBytes(left->getSize()), rightBytes(right->getSize());

                    evaluator->readData(left->getOffset(), leftBytes.data(), leftBytes.size(), left->getSection());
                    evaluator->readData(right->getOffset(), rightBytes.data(), rightBytes.size(), right->getSection());
                    switch (this->getOperator()) {
                        case Token::Operator::BoolEqual:
                            return new ASTNodeLiteral(leftBytes == rightBytes);
                        case Token::Operator::BoolNotEqual:
                            return new ASTNodeLiteral(leftBytes != rightBytes);
                        default:
                            throwInvalidOperandError();
                    }
                },
                [&, this](const std::string &left, auto right) -> ASTNode * {
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
                [&, this](const std::string &left, const std::string &right) -> ASTNode * {
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
                [&, this](const std::string &left, char right) -> ASTNode * {
                    switch (this->getOperator()) {
                        case Token::Operator::Plus:
                            return new ASTNodeLiteral(left + right);
                        default:
                            throwInvalidOperandError();
                    }
                },
                [&, this](char left, const std::string &right) -> ASTNode * {
                    switch (this->getOperator()) {
                        case Token::Operator::Plus:
                            return new ASTNodeLiteral(left + right);
                        default:
                            throwInvalidOperandError();
                    }
                },
                [&, this](auto left, auto right) -> ASTNode * {
                    switch (this->getOperator()) {
                        case Token::Operator::Plus:
                            return new ASTNodeLiteral(left + right);
                        case Token::Operator::Minus:
                            if (left < static_cast<decltype(left)>(right) && std::unsigned_integral<decltype(left)> && std::unsigned_integral<decltype(right)>)
                                return new ASTNodeLiteral(i128(left) - i128(right));
                            else
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