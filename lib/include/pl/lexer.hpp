#pragma once

#include <helpers/types.hpp>

#include <pl/token.hpp>

#include <optional>
#include <string>
#include <vector>

namespace pl {

    class Lexer {
    public:
        Lexer() = default;

        std::optional<std::vector<Token>> lex(const std::string &code);
        const std::optional<PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<PatternLanguageError> m_error;

        [[noreturn]] static void throwLexerError(const std::string &error, u32 lineNumber) {
            throw PatternLanguageError(lineNumber, "Lexer: " + error);
        }
    };

}