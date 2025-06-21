// "new_lexer.hpp"
#pragma once

#include <vector>
#include <pl/core/errors/result.hpp>
#include <pl/core/token.hpp>
#include <lexertl/state_machine.hpp>

namespace pl::core {

void init_new_lexer();

class New_Lexer
{
public:
    static void static_construct();

    New_Lexer() = default;
    hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);

private:
    std::vector<Token> m_tokens;

    static lexertl::state_machine s_sm;
};

} // namespace pl::core
