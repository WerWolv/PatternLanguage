#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeLiteral::ASTNodeLiteral(Token::Literal literal) : ASTNode(), m_literal(std::move(literal)) { }

}