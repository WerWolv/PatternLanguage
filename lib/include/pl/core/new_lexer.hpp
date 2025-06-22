// "new_lexer.hpp"
#pragma once

#include <vector>
#include <pl/core/token.hpp>
#include <pl/core/errors/result.hpp>

namespace pl::core {

void init_new_lexer();

class New_Lexer {
public:
    New_Lexer() = default;
    hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);

private:
    std::vector<Token> m_tokens;
};

} // namespace pl::core
