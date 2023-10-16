#pragma once

#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/token.hpp>

#include <fmt/format.h>

#include <optional>
#include <string>
#include <vector>

namespace pl::core {

    class Lexer : err::ErrorCollector {
    public:
        Lexer() = default;

        CompileResult<std::vector<Token>> lex(const std::string &sourceCode, const std::string &sourceName);

    private:
        static constexpr char integerSeparator = '\'';

        static inline bool isIdentifierCharacter(char c) {
            return std::isalnum(c) || c == '_';
        }

        static inline bool isIntegerCharacter(char c, int base) {
            switch (base) {
                case 16:
                    return std::isxdigit(c);
                case 10:
                    return std::isdigit(c);
                case 8:
                    return c >= '0' && c <= '7';
                case 2:
                    return c == '0' || c == '1';
                default:
                    return false;
            }
        }

        static inline int characterValue(char c) {
            if (c >= '0' && c <= '9') {
                return c - '0';
            } else if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            } else {
                return 0;
            }
        }

        static inline size_t getIntegerLiteralLength(const std::string_view& literal) {
            auto count = literal.find_first_not_of("0123456789ABCDEFabcdef'xXoOpP.uU");
            if (count == std::string_view::npos)
                return literal.size();
            else
                return count;
        }

        inline char peek(size_t p = 1) {
            return m_cursor + p < m_sourceCode.size() ? m_sourceCode[m_cursor + p] : '\0';
        }

        inline bool processToken(auto parserFunction, const std::string_view& identifier) {
            auto token = (this->*parserFunction)(identifier);
            if (token.has_value()) {
                m_tokens.emplace_back(token.value());
                m_cursor += identifier.size();
                return true;
            }
            return false;
        };

        inline err::ErrorLocation location() override {
            return err::ErrorLocation { m_sourceName, m_line, (u32) m_cursor - m_lineBegin };
        }

        std::optional<char> parseCharacter();
        std::optional<Token> parseOperator();
        std::optional<Token> parseSeparator();
        std::optional<Token> parseOneLineDocComment();
        std::optional<Token> parseMultiLineDocComment();
        std::optional<Token> parseKeyword(const std::string_view &identifier);
        std::optional<Token> parseType(const std::string_view &identifier);
        std::optional<Token> parseNamedOperator(const std::string_view &identifier);
        std::optional<Token> parseConstant(const std::string_view &identifier);
        std::optional<Token> parseStringLiteral();
        std::optional<Token::Literal> parseIntegerLiteral(std::string_view literal);

        std::optional<double> parseFloatingPoint(std::string_view literal, char suffix);
        std::optional<u128> parseInteger(std::string_view literal);

        Token makeToken(const Token& token);
        void addToken(const Token& token);

        std::string m_sourceCode;
        std::string m_sourceName;
        std::vector<Token> m_tokens;
        size_t m_cursor{};
        u32 m_line{};
        u32 m_lineBegin{};
    };
}