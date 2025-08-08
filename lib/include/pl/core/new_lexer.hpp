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

    void init_new_lexer();

    class New_Lexer : err::ErrorCollectorExplicitLocation {
    public:
        New_Lexer() = default;
        hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);
        size_t getLongestLineLength() const { return m_longestLineLength; } // TODO: include newline?

    private:
        [[maybe_unused]] std::optional<u128> parseInteger(std::string_view literal, const auto &location);
        [[maybe_unused]] std::optional<double> parseFloatingPoint(std::string_view literal, const char suffix, const auto &location);
        [[maybe_unused]] std::optional<Token::Literal> parseNumericLiteral(std::string_view literal, const auto &location);
        [[maybe_unused]] std::optional<char> parseCharacter(const char* &pchar, const auto &location);
        [[maybe_unused]] std::optional<Token> parseStringLiteral(std::string_view literal, const auto &location);

        std::vector<Token> m_tokens;
        std::size_t m_longestLineLength = 0;
    };

    // Debugging
    void save_compile_results(std::string fn, hlp::CompileResult<std::vector<Token>> &res);

} // namespace pl::core
