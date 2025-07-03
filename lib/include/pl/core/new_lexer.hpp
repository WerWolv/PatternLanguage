// "new_lexer.hpp"
#pragma once

#include <cstddef>
#include <vector>
#include <pl/core/token.hpp>
#include <pl/core/errors/error.hpp>
#include <pl/core/errors/result.hpp>

namespace pl::core {

    void init_new_lexer();

    class New_Lexer : err::ErrorCollectorExplicitLocation {
    public:
        New_Lexer() = default;
        hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);
        size_t getLongestLineLength() const { return m_longestLineLength; } // TODO: include newline?

    private:
        std::vector<Token> m_tokens;
        std::size_t m_longestLineLength = 0;
    };

} // namespace pl::core
