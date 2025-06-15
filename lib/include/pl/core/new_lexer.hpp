// "new_lexer.hpp"
#pragma once

#include <vector>
#include <pl/core/errors/result.hpp>
#include <pl/core/token.hpp>

namespace pl::core {

class New_Lexer
{
public:
    New_Lexer() = default;
    hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);
};

} // namespace pl::core
