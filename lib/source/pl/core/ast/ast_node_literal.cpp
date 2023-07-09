#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeLiteral::ASTNodeLiteral(Token::Literal literal) : ASTNode(), m_literal(std::move(literal)) { }

}