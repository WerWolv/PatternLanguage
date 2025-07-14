#include <pl/core/lexer.hpp>
#include <pl/core/tokens.hpp>
#include <pl/helpers/utils.hpp>
#include <pl/api.hpp>

#include <optional>

/*
TODO:
    There are still potential buffer overruns in here.
    Rewrite the lexer.
*/

namespace pl::core {
    using namespace tkn;

    static constexpr char integerSeparator = '\'';

    static bool isIdentifierCharacter(const char c) {
        return std::isalnum(c) || c == '_';
    }

    static bool isIntegerCharacter(const char c, const int base) {
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

    static int characterValue(const char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }

        return 0;
    }

    static size_t getIntegerLiteralLength(const std::string_view& literal) {
        const auto count = literal.find_first_not_of("0123456789ABCDEFabcdef'xXoOpP.uU+-");
        const std::string_view intLiteral = count == std::string_view::npos ? literal : literal.substr(0, count);
        if (const auto signPos = intLiteral.find_first_of("+-"); signPos != std::string_view::npos && ((literal.at(signPos-1) != 'e' && literal.at(signPos-1) != 'E')  || literal.starts_with("0x")))
            return signPos;
        return intLiteral.size();
    }

    // If this function returns true m_cursor has been advanced.
    // In thus case make sure the bounds are checked.  
    inline bool Lexer::skipLineEnding() {
        char ch = m_sourceCode[m_cursor];
        if (ch == '\n') {
            m_longestLineLength = std::max(m_longestLineLength, m_cursor-m_lineBegin+m_tabCompensation);
            m_tabCompensation = 0;
            m_line++;
            m_lineBegin = ++m_cursor;
            return true;
        } else if (ch == '\r') {
            m_longestLineLength = std::max(m_longestLineLength, m_cursor-m_lineBegin+m_tabCompensation);
            m_tabCompensation = 0;
            m_line++;
            if (++m_cursor<m_sourceCode.size()) {
                ch = m_sourceCode[m_cursor];
                if (ch == '\n')
                    ++m_cursor;
            }
            m_lineBegin = m_cursor;
            return true;
        }

        return false;
    }

    std::optional<char> Lexer::parseCharacter() {
        const char& c = m_sourceCode[m_cursor++];
        if (c == '\\') {
            switch (m_sourceCode[m_cursor++]) {
                case 'a':
                    return '\a';
                case 'b':
                    return '\b';
                case 'f':
                    return '\f';
                case 'n':
                    return '\n';
                case 't':
                    return '\t';
                case 'r':
                    return '\r';
                case '0':
                    return '\0';
                case '\'':
                    return '\'';
                case '"':
                    return '"';
                case '\\':
                    return '\\';
                case 'x': {
                    const char hex[3] = { m_sourceCode[m_cursor], m_sourceCode[m_cursor + 1], 0 };
                    m_cursor += 2;
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        m_errorLength = 2;
                        error("Invalid hex escape sequence: {}", hex);
                        return std::nullopt;
                    }
                }
                case 'u': {
                    const char hex[5] = { m_sourceCode[m_cursor], m_sourceCode[m_cursor + 1], m_sourceCode[m_cursor + 2],
                                    m_sourceCode[m_cursor + 3], 0 };
                    m_cursor += 4;
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        m_errorLength = 4;
                        error("Invalid unicode escape sequence: {}", hex);
                        return std::nullopt;
                    }
                }
                default:
                    m_errorLength = 1;
                    error("Unknown escape sequence: {}", m_sourceCode[m_cursor-1]);
                return std::nullopt;
            }
        }
        return c;
    }

    std::optional<Token> Lexer::parseDirectiveName(const std::string_view &identifier) {
        const auto &directives = Token::Directives();
        if (const auto directiveToken = directives.find(identifier); directiveToken != directives.end()) {
            return makeToken(directiveToken->second, identifier.length());
        }
        m_errorLength = identifier.length();
        error("Unknown directive: {}", identifier);
        return std::nullopt;
    }

    std::optional<Token> Lexer::parseDirectiveValue() {
        std::string result;

        m_cursor++; // Skip space
        auto location = this->location();

        while (!std::isblank(m_sourceCode[m_cursor]) && !std::isspace(m_sourceCode[m_cursor]) && m_sourceCode[m_cursor] != '\0' ) {

            auto character = parseCharacter();
            if (!character.has_value()) {
                return std::nullopt;
            }

            result += character.value();
        }

        skipLineEnding();

        return makeTokenAt(Literal::makeString(result), location, result.size());
    }

    std::optional<Token> Lexer::parseDirectiveArgument() {
        std::string result;

        m_cursor++; // Skip space
        auto location = this->location();

        while (m_sourceCode[m_cursor] != '\n' && m_sourceCode[m_cursor] != '\0') {

            auto character = parseCharacter();
            if (!character.has_value()) {
                return std::nullopt;
            }

            result += character.value();
        }

        skipLineEnding();

        return makeTokenAt(Literal::makeString(result), location, result.size());
    }

    std::optional<Token> Lexer::parseStringLiteral() {
        std::string result;
        auto location = this->location();

        m_cursor++; // Skip opening "

        while (m_sourceCode[m_cursor] != '\"') {
            char c = peek(0);
            if (c == '\n') {
                m_errorLength = 1;
                error("Unexpected newline in string literal");
                m_line++;
                m_lineBegin = m_cursor;
                return std::nullopt;
            }

            if (c == '\0') {
                m_errorLength = 1;
                error("Unexpected end of file in string literal");
                return std::nullopt;
            }

            auto character = parseCharacter();
            if (!character.has_value()) {
                return std::nullopt;
            }

            result += character.value();
        }

        m_cursor++;

        return makeTokenAt(Literal::makeString(result), location, result.size() + 2);
    }

    std::optional<u128> Lexer::parseInteger(std::string_view literal) {
        u8 base = 10;

        u128 value = 0;
        if(literal[0] == '0') {
            if(literal.size() == 1) {
                return 0;
            }
            bool hasPrefix = true;
            switch (literal[1]) {
                case 'x':
                case 'X':
                    base = 16;
                break;
                case 'o':
                case 'O':
                    base = 8;
                break;
                case 'b':
                case 'B':
                    base = 2;
                break;
                default:
                    hasPrefix = false;
                break;
            }
            if (hasPrefix) {
                literal = literal.substr(2);
            }
        }

        for (const char c : literal) {
            if(c == integerSeparator) continue;

            if (!isIntegerCharacter(c, base)) {
                m_errorLength = literal.size();
                error("Invalid integer literal: {}", literal);
                return std::nullopt;
            }
            value = value * base + characterValue(c);
        }

        return value;
    }

    std::optional<double> Lexer::parseFloatingPoint(std::string_view literal, const char suffix) {
        char *end = nullptr;
        double val = std::strtod(literal.data(), &end);

        if(end != literal.data() + literal.size()) {
            m_errorLength = literal.size();
            error("Invalid float literal: {}", literal);
            return std::nullopt;
        }

        switch (suffix) {
            case 'f':
            case 'F':
                return float(val);
            case 'd':
            case 'D':
            default:
                return val;
        }
    }

    std::optional<Token::Literal> Lexer::parseIntegerLiteral(std::string_view literal) {
        // parse a c like numeric literal
        const bool floatSuffix = hlp::stringEndsWithOneOf(literal, { "f", "F", "d", "D" });
        const bool unsignedSuffix = hlp::stringEndsWithOneOf(literal, { "u", "U" });
        const bool isFloat = literal.find('.') != std::string_view::npos
                       || (!literal.starts_with("0x") && floatSuffix);
        const bool isUnsigned = unsignedSuffix;

        if(isFloat) {

            char suffix = 0;
            if(floatSuffix) {
                // remove suffix
                suffix = literal.back();
                literal = literal.substr(0, literal.size() - 1);
            }

            auto floatingPoint = parseFloatingPoint(literal, suffix);

            if(!floatingPoint.has_value()) return std::nullopt;

            return floatingPoint.value();

        }

        if(unsignedSuffix) {
            // remove suffix
            literal = literal.substr(0, literal.size() - 1);
        }

        const auto integer = parseInteger(literal);

        if(!integer.has_value()) return std::nullopt;

        u128 value = integer.value();
        if(isUnsigned) {
            return value;
        }

        return i128(value);
    }

    std::optional<Token> Lexer::parseOneLineComment() {
        auto location = this->location();
        const auto begin = m_cursor;
        m_cursor += 2;

        std::string result;
        while(m_sourceCode[m_cursor] != '\n' && m_sourceCode[m_cursor] != '\0') {
            result += m_sourceCode[m_cursor];
            m_cursor++;
        }
        auto len = m_cursor - begin;

        skipLineEnding();

        return makeTokenAt(Literal::makeComment(true, result), location, len);
    }

    std::optional<Token> Lexer::parseOneLineDocComment() {
        auto location = this->location();
        const auto begin = m_cursor;
        m_cursor += 3;

        std::string result;
        while(m_sourceCode[m_cursor] != '\n' && m_sourceCode[m_cursor] != '\0') {
            result += m_sourceCode[m_cursor];
            m_cursor++;
        }
        auto len = m_cursor - begin;

        skipLineEnding();

        return makeTokenAt(Literal::makeDocComment(false, true, result), location, len);
    }

    std::optional<Token> Lexer::parseMultiLineDocComment() {
        auto location = this->location();
        const auto begin = m_cursor;
        const bool global = peek(2) == '!';
        std::string result;

        m_cursor += 3;
        while (true) {
            skipLineEnding();

            if(m_cursor>=m_sourceCode.size() || peek(0)=='\x00') {
                m_errorLength = 1;
                error("Unexpected end of file while parsing multi line doc comment");
                return std::nullopt;
            }

            if(peek(0) == '*' && peek(1) == '/') {
                m_cursor += 2;
                break;
            }

            result += m_sourceCode[m_cursor++];
        }

        return makeTokenAt(Literal::makeDocComment(global, false, result), location, m_cursor - begin);
    }

    std::optional<Token> Lexer::parseMultiLineComment() {
        auto location = this->location();
        const auto begin = m_cursor;
        std::string result;

        m_cursor += 2;
        while(true) {
            skipLineEnding();

            if(m_cursor>=m_sourceCode.size() || peek(0)=='\x00') {
                m_errorLength = 2;
                error("Unexpected end of file while parsing multi line doc comment");
                return std::nullopt;
            }

            if(peek(0) == '*' && peek(1) == '/') {
                m_cursor += 2;
                break;
            }

            result += m_sourceCode[m_cursor++];
        }

        return makeTokenAt(Literal::makeComment(false, result), location, m_cursor - begin);
    }

    std::optional<Token> Lexer::parseOperator() {
        auto location = this->location();
        const auto begin = m_cursor;
        const auto operators = Token::Operators();
        std::optional<Token> lastMatch = std::nullopt;

        for (int i = 1; i <= Operator::maxOperatorLength; ++i) {
            const auto view = std::string_view { &m_sourceCode[begin], static_cast<size_t>(i) };
            if (auto operatorToken = operators.find(view); operatorToken != operators.end()) {
                m_cursor++;
                lastMatch = operatorToken->second;
            }
        }

        return lastMatch ? makeTokenAt(lastMatch.value(), location, m_cursor - begin) : lastMatch;
    }

    std::optional<Token> Lexer::parseSeparator() {
        auto location = this->location();
        const auto begin = m_cursor;

        if (const auto separatorToken = Token::Separators().find(m_sourceCode[m_cursor]);
            separatorToken != Token::Separators().end()) {
            m_cursor++;
            return makeTokenAt(separatorToken->second, location, m_cursor - begin);
            }

        return std::nullopt;
    }

    std::optional<Token> Lexer::parseKeyword(const std::string_view &identifier) {
        const auto keywords = Token::Keywords();
        if (const auto keywordToken = keywords.find(identifier); keywordToken != keywords.end()) {
            return makeToken(keywordToken->second, identifier.length());
        }
        return std::nullopt;
    }

    std::optional<Token> Lexer::parseType(const std::string_view &identifier) {
        auto types = Token::Types();
        if (const auto typeToken = types.find(identifier); typeToken != types.end()) {
            return makeToken(typeToken->second, identifier.length());
        }
        return std::nullopt;
    }

    std::optional<Token> Lexer::parseNamedOperator(const std::string_view &identifier) {
        auto operators = Token::Operators();
        if (const auto operatorToken = operators.find(identifier); operatorToken != operators.end()) {
            return makeToken(operatorToken->second, identifier.length());
        }
        return std::nullopt;
    }

    std::optional<Token> Lexer::parseConstant(const std::string_view &identifier) {
        if (const auto constantToken = constants.find(identifier); constantToken != constants.end()) {
            return makeToken(Literal::makeNumeric(constantToken->second), identifier.length());
        }
        return std::nullopt;
    }

    Token Lexer::makeToken(const Token &token, const size_t length) {
        auto location = this->location();
        location.length = length;
        return { token.type, token.value, location };
    }

    Token Lexer::makeTokenAt(const Token &token, Location& location, const size_t length) {
        location.length = length;
        return { token.type, token.value, location };
    }

    void Lexer::addToken(const Token &token) {
        m_tokens.emplace_back(token);
    }

    hlp::CompileResult<std::vector<Token>> Lexer::lex(const api::Source *source) {
        this->m_sourceCode = source->content;
        this->m_source = source;
        this->m_cursor = 0;
        this->m_line = 1;
        this->m_lineBegin = 0;
        this->m_longestLineLength = 0;
        this->m_tabCompensation = 0;

        const size_t end = this->m_sourceCode.size();

        m_tokens.clear();

        while (this->m_cursor < end) {
            const char& c = this->m_sourceCode[this->m_cursor];

            if (c == '\x00') {
                m_longestLineLength = std::max(m_longestLineLength, m_cursor-m_lineBegin+m_tabCompensation);
                break; // end of string
            }

            if (std::isblank(c) || std::isspace(c)) {
                if (c == '\t') {
                    u32 column = m_tabCompensation + (m_cursor - m_lineBegin + 1);
                    u32 tabbedColumn = (((column - 1) / tabsize + 1) * tabsize) + 1;
                    m_tabCompensation += tabbedColumn - column - 1;
                    ++m_cursor;
                }
                else if (!skipLineEnding())
                    ++m_cursor;
                continue;
            }

            if(isIdentifierCharacter(c) && !std::isdigit(c)) {
                size_t length = 0;
                while (isIdentifierCharacter(peek(length))) {
                    length++;
                }

                auto identifier = std::string_view { &m_sourceCode[m_cursor], length };

                // process keywords, named operators and types
                if (processToken(&Lexer::parseKeyword, identifier) ||
                    processToken(&Lexer::parseNamedOperator, identifier) ||
                    processToken(&Lexer::parseType, identifier) ||
                    processToken(&Lexer::parseConstant, identifier)) {
                    continue;
                    }

                // not a predefined token, so it must be an identifier
                addToken(makeToken(Literal::makeIdentifier(std::string(identifier)), length));
                this->m_cursor += length;

                continue;
            }

            if(std::isdigit(c)) {
                auto literal = &m_sourceCode[m_cursor];
                size_t size = getIntegerLiteralLength(literal);

                const auto integer = parseIntegerLiteral({ literal, size });

                if(integer.has_value()) {
                    addToken(makeToken(Literal::makeNumeric(integer.value()), size));
                    this->m_cursor += size;
                    continue;
                }

                this->m_cursor += size;
                continue;
            }

            // comment cases
            if(c == '/') {
                const char category = peek(1);
                char type = peek(2);
                if(category == '/') {
                    if(type == '/') {
                        const auto token = parseOneLineDocComment();
                        if(token.has_value()) {
                            addToken(token.value());
                        }
                    } else {
                        const auto token = parseOneLineComment();
                        if(token.has_value()) {
                            addToken(token.value());
                        }
                    }
                    continue;
                }
                if(category == '*') {
                    if (type != '!' && (type != '*' || peek(3) == '/' )) {
                        const auto token = parseMultiLineComment();
                        if(token.has_value())
                            addToken(token.value());
                        continue;
                    }
                    const auto token = parseMultiLineDocComment();
                    if(token.has_value()) {
                        addToken(token.value());
                    }
                    continue;
                }
            }

            const auto operatorToken = parseOperator();
            if (operatorToken.has_value()) {
                addToken(operatorToken.value());
                continue;
            }

            const auto separatorToken = parseSeparator();
            if (separatorToken.has_value()) {
                addToken(separatorToken.value());
                continue;
            }

            if (c == '#' && (m_tokens.empty() ||  m_tokens.back().location.line < m_line)) {
                size_t length = 1;
                u32 line = m_line;
                while (isIdentifierCharacter(peek(length)))
                    length++;
                auto directiveName = std::string_view{&m_sourceCode[m_cursor], length};

                if (processToken(&Lexer::parseDirectiveName, directiveName)) {
                    Token::Directive directive = get<Token::Directive>(m_tokens.back().value);
                    if (m_line != line || directive == Token::Directive::Define || directive == Token::Directive::Undef ||
                         peek(0) == 0  || directive == Token::Directive::IfDef  || directive == Token::Directive::IfNDef ||
                         directive == Token::Directive::EndIf)
                        continue;
                    if (skipLineEnding())
                        continue;
                    auto directiveValue = parseDirectiveValue();
                    if (directiveValue.has_value()) {
                        addToken(directiveValue.value());
                        if (m_line != line || peek(0) == 0)
                            continue;
                        if (skipLineEnding())
                            continue;
                        directiveValue = parseDirectiveArgument();
                        if (directiveValue.has_value()) {
                            addToken(directiveValue.value());
                        }
                    }
                    continue;
                }
            }

            // literals
            if (c == '"') {
                const auto string = parseStringLiteral();

                if (string.has_value()) {
                    addToken(string.value());
                    continue;
                }
            } else if(c == '\'') {
                auto location = this->location();
                const auto begin = m_cursor;
                m_cursor++; // skip opening '
                const auto character = parseCharacter();

                if (character.has_value()) {
                    if(m_sourceCode[m_cursor] != '\'') {
                        m_errorLength = 1;
                        error("Expected closing '");
                        continue;
                    }

                    m_cursor++; // skip closing '

                    addToken(makeTokenAt(Literal::makeNumeric(character.value()), location, m_cursor - begin));
                    continue;
                }
            } else {
                m_errorLength = 1;
                error("Unexpected character: {}", c);
                m_cursor++;

                break;
            }

            m_cursor++;
        }
        m_longestLineLength = std::max(m_longestLineLength, m_cursor - m_lineBegin);
        addToken(makeToken(Separator::EndOfProgram, 0));

        return { m_tokens, collectErrors() };
    }

    // Note: if m_cursor is out of bounds we return ''\0''.
    inline char Lexer::peek(const size_t p) const {
        return m_cursor + p < m_sourceCode.size() ? m_sourceCode[m_cursor + p] : '\0';
    }

    bool Lexer::processToken(auto parserFunction, const std::string_view& identifier) {
        const auto token = (this->*parserFunction)(identifier);
        if (token.has_value()) {
            m_tokens.emplace_back(token.value());
            m_cursor += identifier.size();
            return true;
        }
        return false;
    }

    Location Lexer::location()
    {
        u32 column = m_tabCompensation + m_cursor - m_lineBegin + 1;
        return Location{m_source, m_line, column, m_errorLength};
    }
}