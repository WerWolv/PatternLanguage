#pragma once

#include <helpers/types.hpp>

#include <pl/token.hpp>

#include <optional>
#include <string>
#include <vector>

#include <pl/errors/lexer_errors.hpp>

namespace pl {

    class Lexer {
    public:
        Lexer() = default;

        std::optional<std::vector<Token>> lex(const std::string &sourceCode, const std::string &preprocessedSourceCode);
        const std::optional<err::Error::Exception> &getError() { return this->m_error; }

    private:
        std::optional<err::Error::Exception> m_error;
    };

}