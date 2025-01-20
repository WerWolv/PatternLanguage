#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeLiteral : public ASTNode {
    public:
        explicit ASTNodeLiteral(Token::Literal literal);
#ifndef LIBWOLV_BUILTIN_UINT128
        explicit ASTNodeLiteral(int value)
                : ASTNodeLiteral(i128(value)) { }
#endif // LIBWOLV_BUILTIN_UINT128
        ASTNodeLiteral(const ASTNodeLiteral &) = default;

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(*this));
        }

        [[nodiscard]] const auto &getValue() const {
            return this->m_literal;
        }

    private:
        Token::Literal m_literal;
    };

}