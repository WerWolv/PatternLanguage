// "Lexer.hpp"
#pragma once

#include <cstddef>
#include <optional>
#include <vector>
#include <pl/core/token.hpp>
#include <pl/core/errors/error.hpp>
#include <pl/core/errors/result.hpp>
// Debugging
#include <string>

namespace pl::core {

    class Lexer : err::ErrorCollectorExplicitLocation {
    public:
        Lexer();

        void reset();

        hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);
        size_t getLongestLineLength() const { return m_longestLineLength; }

    private:
        std::optional<Token::Literal> parseInteger(std::string_view literal, const auto &location);
        std::optional<double> parseFloatingPoint(std::string_view literal, const char suffix, const auto &location);
        std::optional<char> parseCharacter(const char* &pchar, const char* e, const auto &location);
        std::optional<Token> parseStringLiteral(std::string_view literal, const auto &location);

        std::vector<Token> m_tokens;
        std::size_t m_longestLineLength = 0;
    };

} // namespace pl::core
