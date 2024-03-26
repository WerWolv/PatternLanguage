#pragma once

#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/token.hpp>

#include <fmt/format.h>

#include <optional>
#include <string>
#include <vector>

#include <pl/core/errors/result.hpp>

namespace pl::core {

    class Lexer : err::ErrorCollector {
    public:
        Lexer() = default;

        hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);

    private:
        [[nodiscard]] char peek(size_t p = 1) const;
        bool processToken(auto parserFunction, const std::string_view& identifier);
        Location location() override;

        std::optional<char> parseCharacter();
        std::optional<Token> parseOperator();
        std::optional<Token> parseSeparator();
        std::optional<Token> parseOneLineComment();
        std::optional<Token> parseOneLineDocComment();
        std::optional<Token> parseMultiLineComment();
        std::optional<Token> parseMultiLineDocComment();
        std::optional<Token> parseKeyword(const std::string_view &identifier);
        std::optional<Token> parseType(const std::string_view &identifier);
        std::optional<Token> parseDirectiveName(const std::string_view &identifier);
        std::optional<Token> parseNamedOperator(const std::string_view &identifier);
        std::optional<Token> parseConstant(const std::string_view &identifier);
        std::optional<Token> parseStringLiteral();
        std::optional<Token> parseDirectiveArgument();
        std::optional<Token> parseDirectiveValue();
        std::optional<Token::Literal> parseIntegerLiteral(std::string_view literal);

        std::optional<double> parseFloatingPoint(std::string_view literal, char suffix);
        std::optional<u128> parseInteger(std::string_view literal);

        Token makeToken(const Token& token, size_t length = 1);
        static Token makeTokenAt(const Token& token, Location& location, size_t length = 1);
        void addToken(const Token& token);

        std::string m_sourceCode;
        const api::Source* m_source = nullptr;
        std::vector<Token> m_tokens;
        size_t m_cursor = 0;
        u32 m_line = 0;
        u32 m_lineBegin = 0;
    };
}