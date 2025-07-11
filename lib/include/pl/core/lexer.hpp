#pragma once

#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/token.hpp>

#include <fmt/core.h>

#include <optional>
#include <string>
#include <vector>

#include <pl/core/errors/result.hpp>

namespace pl::core {

    class Lexer : err::ErrorCollector {
    public:
        Lexer() = default;

        hlp::CompileResult<std::vector<Token>> lex(const api::Source *source);
        size_t getLongestLineLength() const { 
            // 'getLongestLineLength' is used my the pattern editor to control
            // the x-scrolling range. Adding two makes the whole longest line visible.
            // Not sure why we have to do this.
            return m_longestLineLength+2; 
        }

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

        bool skipLineEnding() {
            char ch = m_sourceCode[m_cursor];
            if (ch == '\n') {
                m_longestLineLength = std::max(m_longestLineLength, m_cursor-m_lineBegin+m_tabCompensation);
                m_tabCompensation = 0;
                m_line++;
                m_lineBegin = ++m_cursor;
                return true;
            }
            else if (ch == '\r') {
                m_longestLineLength = std::max(m_longestLineLength, m_cursor-m_lineBegin+m_tabCompensation);
                m_tabCompensation = 0;
                m_line++;
                ch = m_sourceCode[++m_cursor];
                if (ch == '\n')
                    ++m_cursor;
                m_lineBegin = m_cursor;
                return true;
            }

            return false;
        }

        static constexpr int tabsize = 4;

        std::string m_sourceCode;
        const api::Source* m_source = nullptr;
        std::vector<Token> m_tokens;
        size_t m_cursor = 0;
        u32 m_line = 0;
        u32 m_tabCompensation = 0;
        u32 m_lineBegin = 0;
        size_t m_longestLineLength = 0;
        u32 m_errorLength = 0;
    };
}