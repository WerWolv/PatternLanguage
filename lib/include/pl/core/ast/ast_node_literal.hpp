#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeLiteral : public ASTNode {
    public:
        explicit ASTNodeLiteral(Token::Literal literal) : ASTNode(), m_literal(std::move(literal)) { }

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