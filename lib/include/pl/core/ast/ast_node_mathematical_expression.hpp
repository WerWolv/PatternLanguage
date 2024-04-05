#pragma once

#include <pl/core/ast/ast_node.hpp>

#include <wolv/utils/core.hpp>

namespace pl::core::ast {

#define FLOAT_BIT_OPERATION(name)                                                       \
    auto name(pl::floating_point auto left, auto right) const {                         \
        wolv::util::unused(left, right);                                                \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this->getLocation());                                                       \
        return 0;                                                                       \
    }                                                                                   \
    auto name(auto left, pl::floating_point auto right) const {                         \
        wolv::util::unused(left, right);                                                \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this->getLocation());                                                       \
        return 0;                                                                       \
    }                                                                                   \
    auto name(pl::floating_point auto left, pl::floating_point auto right) const {      \
        wolv::util::unused(left, right);                                                \
        err::E0002.throwError(                                                          \
            "Invalid floating point operation.",                                        \
            "This operation doesn't make sense to be used with floating point values.", \
            this->getLocation());                                                       \
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
        ASTNodeMathematicalExpression(std::unique_ptr<ASTNode> &&left, std::unique_ptr<ASTNode> &&right, Token::Operator op);
        ASTNodeMathematicalExpression(const ASTNodeMathematicalExpression &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMathematicalExpression(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

        [[nodiscard]] const std::unique_ptr<ASTNode> &getLeftOperand() const { return this->m_left; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getRightOperand() const { return this->m_right; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

    private:
        std::unique_ptr<ASTNode> m_left, m_right;
        Token::Operator m_operator;
    };

}