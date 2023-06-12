#include <pl/core/ast/ast_node_parameter_pack.hpp>

namespace pl::core::ast {

    ASTNodeParameterPack::ASTNodeParameterPack(std::vector<Token::Literal> &&values) : m_values(std::move(values)) { }

}