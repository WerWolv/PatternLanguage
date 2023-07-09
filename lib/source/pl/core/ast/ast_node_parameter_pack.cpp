#include <pl/core/ast/ast_node_parameter_pack.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace pl::core::ast {

    ASTNodeParameterPack::ASTNodeParameterPack(std::vector<Token::Literal> &&values) : m_values(std::move(values)) { }

}