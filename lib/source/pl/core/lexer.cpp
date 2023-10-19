#include <pl/core/lexer.hpp>
#include <pl/core/tokens.hpp>
#include <pl/helpers/utils.hpp>

#include <optional>

using namespace pl;
using namespace pl::core;
using namespace tkn;

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
                char hex[3] = { m_sourceCode[m_cursor], m_sourceCode[m_cursor + 1], 0 };
                m_cursor += 2;
                return static_cast<char>(std::stoul(hex, nullptr, 16));
            }
            case 'u': {
                char hex[5] = { m_sourceCode[m_cursor], m_sourceCode[m_cursor + 1], m_sourceCode[m_cursor + 2],
                                m_sourceCode[m_cursor + 3], 0 };
                m_cursor += 4;
                return static_cast<char>(std::stoul(hex, nullptr, 16));
            }
            default:
                error("Unknown escape sequence: {}", m_sourceCode[m_cursor-1]);
                return std::nullopt;
        }
    } else {
        return c;
    }
}

std::optional<Token> Lexer::parseStringLiteral() {
    std::string result;

    m_cursor++;
    while(m_sourceCode[m_cursor] != '\"') {

        auto character = parseCharacter();

        if(character.has_value()) {
            result += character.value();
        } else {
            return std::nullopt;
        }

        if(m_cursor > m_sourceCode.size()) {
            error("Unexpected end of string literal");
            return std::nullopt;
        }
    }

    m_cursor++;

    return makeToken(Literal::makeString(result));
}

std::optional<u128> Lexer::parseInteger(std::string_view literal) {
    u8 base = 10;

    u128 value = 0;
    if(literal[0] == '0') {
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

    for (char c : literal) {
        if(c == integerSeparator) continue;

        if (!isIntegerCharacter(c, base)) {
            error("Invalid integer literal: {}", literal);
            return std::nullopt;
        }
        value = value * base + characterValue(c);
    }

    return value;
}

std::optional<double> Lexer::parseFloatingPoint(std::string_view literal, char suffix) {
    char *end = nullptr;
    double val = std::strtod(literal.data(), &end);

    if(end != literal.data() + literal.size()) {
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
    bool floatSuffix = hlp::string_ends_with_one_of(literal, { "f", "F", "d", "D" });
    bool unsignedSuffix = hlp::string_ends_with_one_of(literal, { "u", "U" });
    bool isFloat = literal.find('.') != std::string_view::npos
                   || (!literal.starts_with("0x") && floatSuffix);
    bool isUnsigned = unsignedSuffix;

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

    } else {

        if(unsignedSuffix) {
            // remove suffix
            literal = literal.substr(0, literal.size() - 1);
        }

        auto integer = parseInteger(literal);

        if(!integer.has_value()) return std::nullopt;

        u128 value = integer.value();
        if(isUnsigned) {
            return value;
        } else {
            return i128(value);
        }


    }
}

std::optional<Token> Lexer::parseOneLineDocComment() {
    m_cursor += 3;
    std::string result;
    while(m_sourceCode[m_cursor] != '\n') {
        result += m_sourceCode[m_cursor++];
        if(peek(1) == 0) {
            error("Unexpected end of file while parsing one line doc comment");
            return std::nullopt;
        }
    }
    return makeToken(Literal::makeDocComment(false, result));
}

std::optional<Token> Lexer::parseMultiLineDocComment() {
    bool global = peek(2) == '!';
    m_cursor += 3;
    std::string result;
    while(true) {
        if(peek(0) == '\n') {
            m_line++;
            m_lineBegin = m_cursor;
        }
        if(peek(1) == 0) {
            error("Unexpected end of file while parsing multi line doc comment");
            return std::nullopt;
        }
        if(peek(1) == '*' && peek(2) == '/') {
            m_cursor += 2;
            break;
        }
        result += m_sourceCode[m_cursor++];
    }
    return makeToken(Literal::makeDocComment(global, result));
}

std::optional<Token> Lexer::parseOperator() {
    auto operators = Token::operators();
    std::optional<Token> lastMatch = std::nullopt;
    size_t begin = m_cursor;
    for (int i = 1; i <= Operator::maxOperatorLength; ++i) {
        const auto view = std::string_view { &m_sourceCode[begin], (size_t) i };
        auto operatorToken = operators.find(view);
        if (operatorToken != operators.end()) {
            m_cursor++;
            lastMatch = operatorToken->second;
        }
    }
    if(lastMatch.has_value()) {
        return makeToken(lastMatch.value());
    }
    return lastMatch;
}

std::optional<Token> Lexer::parseSeparator() {
    auto separators = Token::separators();
    auto separatorToken = separators.find(m_sourceCode[m_cursor]);
    if (separatorToken != separators.end()) {
        m_cursor++;
        return makeToken(separatorToken->second);
    }
    return std::nullopt;
}

std::optional<Token> Lexer::parseKeyword(const std::string_view &identifier) {
    auto keywords = Token::keywords();
    auto keywordToken = keywords.find(identifier);
    if (keywordToken != keywords.end()) {
        return makeToken(keywordToken->second);
    }
    return std::nullopt;
}

std::optional<Token> Lexer::parseType(const std::string_view &identifier) {
    auto types = Token::types();
    auto typeToken = types.find(identifier);
    if (typeToken != types.end()) {
        return makeToken(typeToken->second);
    }
    return std::nullopt;
}

std::optional<Token> Lexer::parseNamedOperator(const std::string_view &identifier) {
    auto operatorToken = Token::operators().find(identifier);
    if (operatorToken != Token::operators().end()) {
        return makeToken(operatorToken->second);
    }
    return std::nullopt;
}

std::optional<Token> Lexer::parseConstant(const std::string_view &identifier) {
    auto constantToken = tkn::constants.find(identifier);
    if (constantToken != tkn::constants.end()) {
        return makeToken(Literal::makeNumeric(constantToken->second));
    }
    return std::nullopt;
}

Token Lexer::makeToken(const Token &token) {
    return Token(token.type, token.value, { m_sourceName, m_line, (u32) m_cursor - m_lineBegin });
}

void Lexer::addToken(const Token &token) {
    m_tokens.emplace_back(token);
}

CompileResult<std::vector<Token>> Lexer::lex(const std::string &sourceCode, const std::string &sourceName) {
    this->m_sourceCode = sourceCode;
    this->m_sourceName = sourceName;
    this->m_cursor = 0;
    this->m_line = 1;

    size_t end = this->m_sourceCode.size();

    m_tokens.clear();
    m_errors.clear();

    while(this->m_cursor < end) {
        const char& c = this->m_sourceCode[this->m_cursor];

        if (c == 0) break; // end of string

        if (std::isblank(c) || std::isspace(c)) {
            if(c == '\n') {
                m_line++;
                m_lineBegin = m_cursor;
            }
            m_cursor++;
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
            addToken(makeToken(Literal::makeIdentifier(std::string(identifier))));
            this->m_cursor += length;

            continue;
        }

        if(std::isdigit(c)) {
            auto literal = &m_sourceCode[m_cursor];
            size_t size = getIntegerLiteralLength(literal);

            auto integer = parseIntegerLiteral({ literal, size });

            if(integer.has_value()) {
                addToken(makeToken(Literal::makeNumeric(integer.value())));
                this->m_cursor += size;
                continue;
            }

            this->m_cursor += size;
            continue;
        }

        // comment cases
        if(c == '/') {
            char category = peek(1);
            char type = peek(2);
            if(category == '/') {
                if(type == '/') {
                    auto token = parseOneLineDocComment();
                    if(token.has_value()) {
                        addToken(token.value());
                    }
                } else {
                    error("Expected '/' after '//' but got '//{}'", type);
                }
                continue;
            } else if(category == '*') {
                if(type != '!' && type != '*') {
                    error("Invalid documentation comment. Expected '/**' or '/*!', got '/*{}", type);
                    continue;
                }
                auto token = parseMultiLineDocComment();
                if(token.has_value()) {
                    addToken(token.value());
                }
                m_cursor++; // skip closing /
                continue;
            }
        }

        auto operatorToken = parseOperator();
        if (operatorToken.has_value()) {
            addToken(operatorToken.value());
            continue;
        }

        auto separatorToken = parseSeparator();
        if (separatorToken.has_value()) {
            addToken(separatorToken.value());
            continue;
        }

        // literals
        if (c == '"') {
            auto string = parseStringLiteral();

            if (string.has_value()) {
                addToken(string.value());
                continue;
            }
        } else if(c == '\'') {
            m_cursor++; // skip opening '
            auto character = parseCharacter();

            if (character.has_value()) {
                if(m_sourceCode[m_cursor] != '\'') {
                    error("Expected closing '");
                    continue;
                }

                m_cursor++; // skip closing '

                addToken(makeToken(Literal::makeNumeric(character.value())));
                continue;
            }
        } else {
            error("Unexpected character: {}", c);
        }

        m_cursor++;
    }

    addToken(makeToken(Separator::EndOfProgram));

    return { m_tokens, m_errors };
}

inline char Lexer::peek(size_t p) const {
    return m_cursor + p < m_sourceCode.size() ? m_sourceCode[m_cursor + p] : '\0';
}

inline bool Lexer::processToken(auto parserFunction, const std::string_view& identifier) {
    auto token = (this->*parserFunction)(identifier);
    if (token.has_value()) {
        m_tokens.emplace_back(token.value());
        m_cursor += identifier.size();
        return true;
    }
    return false;
};

inline Location Lexer::location() {
    return Location { m_sourceName, m_line, (u32) m_cursor - m_lineBegin };
}