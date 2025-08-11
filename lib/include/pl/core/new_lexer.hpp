// "new_lexer.hpp"
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

    class New_Lexer : err::ErrorCollectorExplicitLocation {
    public:
        New_Lexer();
        hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);
        size_t getLongestLineLength() const { return m_longestLineLength; } // TODO: include newline?

    private:
        std::optional<u128> parseInteger(std::string_view literal, const auto &location);
        std::optional<double> parseFloatingPoint(std::string_view literal, const char suffix, const auto &location);
        std::optional<char> parseCharacter(const char* &pchar, const auto &location);
        std::optional<Token> parseStringLiteral(std::string_view literal, const auto &location);

        std::vector<Token> m_tokens;
        std::size_t m_longestLineLength = 0;
    };

    // Debugging
    void compareCompileResults(const hlp::CompileResult<std::vector<Token>> &o, const hlp::CompileResult<std::vector<Token>> &n);
    void saveCompileResults(const std::string &fn, const hlp::CompileResult<std::vector<Token>> &res);

} // namespace pl::core
