#pragma once

#include <pl/helpers/types.hpp>

#include <pl/core/token.hpp>

#include <optional>
#include <string>
#include <vector>

#include <pl/core/errors/lexer_errors.hpp>

namespace pl::core {

    class Lexer {
    public:
        Lexer() = default;

        std::optional<std::vector<Token>> lex(const std::string &sourceCode, const std::string &preprocessedSourceCode);
        const std::optional<err::PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<err::PatternLanguageError> m_error;
    };

}