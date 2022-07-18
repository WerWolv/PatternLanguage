#include "pl/lexer.hpp"

#include <algorithm>
#include <charconv>
#include <functional>
#include <optional>
#include <vector>

namespace pl {

    std::string matchTillInvalid(const char *characters, const std::function<bool(char)> &predicate) {
        std::string ret;

        while (*characters != 0x00) {
            ret += *characters;
            characters++;

            if (!predicate(*characters))
                break;
        }

        return ret;
    }

    bool isIdentifierCharacter(char c) {
        return std::isalnum(c) || c == '_';
    }

    size_t getIntegerLiteralLength(std::string_view string) {
        auto count = string.find_first_not_of("0123456789ABCDEFabcdef'xXoOpP.uU");
        if (count == std::string_view::npos)
            return string.size();
        else
            return count;
    }

    std::optional<Token::Literal> lexIntegerLiteral(std::string_view string) {
        bool hasFloatSuffix = string.ends_with('D') || string.ends_with('F') || string.ends_with('d') || string.ends_with('f');
        bool isFloat        = std::count(string.begin(), string.end(), '.') == 1 || (!string.starts_with("0x") && hasFloatSuffix);

        if (isFloat) {
            // Parse double
            char suffix = 0x00;
            if (hasFloatSuffix) {
                suffix = string.back();
                string = string.substr(0, string.length() - 1);
            }

            char *end    = nullptr;
            double value = std::strtod(string.begin(), &end);

            if (end == string.end()) {
                switch (suffix) {
                    case 'd':
                    case 'D':
                        return double(value);
                    case 'f':
                    case 'F':
                        return float(value);
                    default:
                        return value;
                }
            }
        } else {
            bool isUnsigned = false;
            if (string.ends_with('U') || string.ends_with('u')) {
                isUnsigned = true;
                string     = string.substr(0, string.length() - 1);
            }

            u8 prefixOffset = 0;
            u8 base         = 10;

            if (string.starts_with("0x") || string.starts_with("0X")) {
                // Parse hexadecimal
                prefixOffset = 2;
                base         = 16;
            } else if (string.starts_with("0o") || string.starts_with("0O")) {
                // Parse octal
                prefixOffset = 2;
                base         = 8;
            } else if (string.starts_with("0b") || string.starts_with("0B")) {
                // Parse binary
                prefixOffset = 2;
                base         = 2;
            } else {
                // Parse decimal
                prefixOffset = 0;
                base         = 10;
            }

            u128 value = 0x00;
            for (char c : string.substr(prefixOffset)) {
                value *= base;
                value += [&] {
                    if (c >= '0' && c <= '9') return c - '0';
                    else if (c >= 'A' && c <= 'F') return 0xA + (c - 'A');
                    else if (c >= 'a' && c <= 'f') return 0xA + (c - 'a');
                    else return 0x00;
                }();
            }

            if (isUnsigned)
                return value;
            else
                return i128(value);
        }

        return std::nullopt;
    }

    std::optional<Token::Literal> lexIntegerLiteralWithSeparator(std::string_view string) {

        if (string.starts_with('\'') || string.ends_with('\''))
            return std::nullopt;
        else if (string.find('\'') == std::string_view::npos)
            return lexIntegerLiteral(string);
        else {
            auto preprocessedString = std::string(string);
            preprocessedString.erase(std::remove(preprocessedString.begin(), preprocessedString.end(), '\''), preprocessedString.end());
            return lexIntegerLiteral(preprocessedString);
        }
    }

    std::optional<std::pair<char, size_t>> getCharacter(const std::string &string) {

        if (string.length() < 1)
            return std::nullopt;

        // Escape sequences
        if (string[0] == '\\') {

            if (string.length() < 2)
                return std::nullopt;

            // Handle simple escape sequences
            switch (string[1]) {
                case 'a':
                    return {
                        {'\a', 2}
                    };
                case 'b':
                    return {
                        {'\b', 2}
                    };
                case 'f':
                    return {
                        {'\f', 2}
                    };
                case 'n':
                    return {
                        {'\n', 2}
                    };
                case 'r':
                    return {
                        {'\r', 2}
                    };
                case 't':
                    return {
                        {'\t', 2}
                    };
                case 'v':
                    return {
                        {'\v', 2}
                    };
                case '\\':
                    return {
                        {'\\', 2}
                    };
                case '\'':
                    return {
                        {'\'', 2}
                    };
                case '\"':
                    return {
                        {'\"', 2}
                    };
            }

            // Hexadecimal number
            if (string[1] == 'x') {
                if (string.length() < 4)
                    return std::nullopt;

                if (!isxdigit(string[2]) || !isxdigit(string[3]))
                    return std::nullopt;

                return {
                    { std::strtoul(&string[2], nullptr, 16), 4 }
                };
            }

            // Octal number
            if (string[1] == 'o') {
                if (string.length() != 5)
                    return {};

                if (string[2] < '0' || string[2] > '7' || string[3] < '0' || string[3] > '7' || string[4] < '0' || string[4] > '7')
                    return {};

                return {
                    {std::strtoul(&string[2], nullptr, 8), 5}
                };
            }

            return std::nullopt;
        } else return {
            {string[0], 1}
 };
    }

    std::optional<std::pair<std::string, size_t>> getStringLiteral(const std::string &string) {
        if (!string.starts_with('\"'))
            return {};

        size_t size = 1;

        std::string result;
        while (string[size] != '\"') {
            auto character = getCharacter(string.substr(size));

            if (!character.has_value())
                return {};

            auto &[c, charSize] = character.value();

            result += c;
            size += charSize;

            if (size >= string.length())
                return {};
        }

        return {
            {result, size + 1}
        };
    }

    std::optional<std::pair<char, size_t>> getCharacterLiteral(const std::string &string) {
        if (string.empty())
            return {};

        if (string[0] != '\'')
            return {};


        auto character = getCharacter(string.substr(1));

        if (!character.has_value())
            return {};

        auto &[c, charSize] = character.value();

        if (string.length() >= charSize + 2 && string[charSize + 1] != '\'')
            return {};

        return {
            {c, charSize + 2}
        };
    }

    std::optional<std::vector<Token>> Lexer::lex(const std::string &code) {
        std::vector<Token> tokens;
        u32 offset = 0;

        u32 line = 1;
        u32 lineStartOffset = 0;

        try {

            while (offset < code.length()) {
                const char &c = code[offset];

                if (c == 0x00)
                    break;

                if (std::isblank(c) || std::isspace(c)) {
                    if (code[offset] == '\n') {
                        line++;
                        lineStartOffset = offset + 1;
                    }
                    offset += 1;
                } else if (c == ';') {
                    tokens.push_back(tkn::Separator::Semicolon);
                    offset += 1;
                } else if (c == '(') {
                    tokens.push_back(tkn::Separator::LeftParenthesis);
                    offset += 1;
                } else if (c == ')') {
                    tokens.push_back(tkn::Separator::RightParenthesis);
                    offset += 1;
                } else if (c == '{') {
                    tokens.push_back(tkn::Separator::LeftBrace);
                    offset += 1;
                } else if (c == '}') {
                    tokens.push_back(tkn::Separator::RightBrace);
                    offset += 1;
                } else if (c == '[') {
                    tokens.push_back(tkn::Separator::LeftBracket);
                    offset += 1;
                } else if (c == ']') {
                    tokens.push_back(tkn::Separator::RightBracket);
                    offset += 1;
                } else if (c == ',') {
                    tokens.push_back(tkn::Separator::Comma);
                    offset += 1;
                } else if (c == '.') {
                    tokens.push_back(tkn::Separator::Dot);
                    offset += 1;
                } else if (code.substr(offset, 2) == "::") {
                    tokens.push_back(tkn::Operator::ScopeResolution);
                    offset += 2;
                } else if (c == '@') {
                    tokens.push_back(tkn::Operator::At);
                    offset += 1;
                } else if (code.substr(offset, 2) == "==") {
                    tokens.push_back(tkn::Operator::BoolEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == "!=") {
                    tokens.push_back(tkn::Operator::BoolNotEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == ">=") {
                    tokens.push_back(tkn::Operator::BoolGreaterThanOrEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == "<=") {
                    tokens.push_back(tkn::Operator::BoolLessThanOrEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == "&&") {
                    tokens.push_back(tkn::Operator::BoolAnd);
                    offset += 2;
                } else if (code.substr(offset, 2) == "||") {
                    tokens.push_back(tkn::Operator::BoolOr);
                    offset += 2;
                } else if (code.substr(offset, 2) == "^^") {
                    tokens.push_back(tkn::Operator::BoolXor);
                    offset += 2;
                } else if (c == '=') {
                    tokens.push_back(tkn::Operator::Assign);
                    offset += 1;
                } else if (c == ':') {
                    tokens.push_back(tkn::Operator::Colon);
                    offset += 1;
                } else if (c == '+') {
                    tokens.push_back(tkn::Operator::Plus);
                    offset += 1;
                } else if (c == '-') {
                    tokens.push_back(tkn::Operator::Minus);
                    offset += 1;
                } else if (c == '*') {
                    tokens.push_back(tkn::Operator::Star);
                    offset += 1;
                } else if (c == '/') {
                    tokens.push_back(tkn::Operator::Slash);
                    offset += 1;
                } else if (c == '%') {
                    tokens.push_back(tkn::Operator::Percent);
                    offset += 1;
                } else if (code.substr(offset, 2) == "<<") {
                    tokens.push_back(tkn::Operator::LeftShift);
                    offset += 2;
                } else if (code.substr(offset, 2) == ">>") {
                    tokens.push_back(tkn::Operator::RightShift);
                    offset += 2;
                } else if (c == '>') {
                    tokens.push_back(tkn::Operator::BoolGreaterThan);
                    offset += 1;
                } else if (c == '<') {
                    tokens.push_back(tkn::Operator::BoolLessThan);
                    offset += 1;
                } else if (c == '!') {
                    tokens.push_back(tkn::Operator::BoolNot);
                    offset += 1;
                } else if (c == '|') {
                    tokens.push_back(tkn::Operator::BitOr);
                    offset += 1;
                } else if (c == '&') {
                    tokens.push_back(tkn::Operator::BitAnd);
                    offset += 1;
                } else if (c == '^') {
                    tokens.push_back(tkn::Operator::BitXor);
                    offset += 1;
                } else if (c == '~') {
                    tokens.push_back(tkn::Operator::BitNot);
                    offset += 1;
                } else if (c == '?') {
                    tokens.push_back(tkn::Operator::TernaryConditional);
                    offset += 1;
                } else if (c == '$') {
                    tokens.push_back(tkn::Operator::Dollar);
                    offset += 1;
                } else if (code.substr(offset, 9) == "addressof" && !isIdentifierCharacter(code[offset + 9])) {
                    tokens.push_back(tkn::Operator::AddressOf);
                    offset += 9;
                } else if (code.substr(offset, 6) == "sizeof" && !isIdentifierCharacter(code[offset + 6])) {
                    tokens.push_back(tkn::Operator::SizeOf);
                    offset += 6;
                } else if (c == '\'') {
                    auto lexedCharacter = getCharacterLiteral(code.substr(offset));

                    if (!lexedCharacter.has_value())
                        throwLexerError("invalid character literal", line);

                    auto [character, charSize] = lexedCharacter.value();

                    tokens.push_back(tkn::Literal::Numeric(character));
                    offset += charSize;
                } else if (c == '\"') {
                    auto string = getStringLiteral(code.substr(offset));

                    if (!string.has_value())
                        throwLexerError("invalid string literal", line);

                    auto [s, stringSize] = string.value();

                    tokens.push_back(tkn::Literal::String(s));
                    offset += stringSize;
                } else if (isIdentifierCharacter(c) && !std::isdigit(c)) {
                    std::string identifier = matchTillInvalid(&code[offset], isIdentifierCharacter);

                    // Check for reserved keywords

                    if (identifier == "struct")
                        tokens.push_back(tkn::Keyword::Struct);
                    else if (identifier == "union")
                        tokens.push_back(tkn::Keyword::Union);
                    else if (identifier == "using")
                        tokens.push_back(tkn::Keyword::Using);
                    else if (identifier == "enum")
                        tokens.push_back(tkn::Keyword::Enum);
                    else if (identifier == "bitfield")
                        tokens.push_back(tkn::Keyword::Bitfield);
                    else if (identifier == "be")
                        tokens.push_back(tkn::Keyword::BigEndian);
                    else if (identifier == "le")
                        tokens.push_back(tkn::Keyword::LittleEndian);
                    else if (identifier == "if")
                        tokens.push_back(tkn::Keyword::If);
                    else if (identifier == "else")
                        tokens.push_back(tkn::Keyword::Else);
                    else if (identifier == "false")
                        tokens.push_back(tkn::Literal::Numeric(false));
                    else if (identifier == "true")
                        tokens.push_back(tkn::Literal::Numeric(true));
                    else if (identifier == "parent")
                        tokens.push_back(tkn::Keyword::Parent);
                    else if (identifier == "this")
                        tokens.push_back(tkn::Keyword::This);
                    else if (identifier == "while")
                        tokens.push_back(tkn::Keyword::While);
                    else if (identifier == "for")
                        tokens.push_back(tkn::Keyword::For);
                    else if (identifier == "fn")
                        tokens.push_back(tkn::Keyword::Function);
                    else if (identifier == "return")
                        tokens.push_back(tkn::Keyword::Return);
                    else if (identifier == "namespace")
                        tokens.push_back(tkn::Keyword::Namespace);
                    else if (identifier == "in")
                        tokens.push_back(tkn::Keyword::In);
                    else if (identifier == "out")
                        tokens.push_back(tkn::Keyword::Out);
                    else if (identifier == "break")
                        tokens.push_back(tkn::Keyword::Break);
                    else if (identifier == "continue")
                        tokens.push_back(tkn::Keyword::Continue);

                    // Check for built-in types
                    else if (identifier == "u8")
                        tokens.push_back(tkn::ValueType::Unsigned8Bit);
                    else if (identifier == "s8")
                        tokens.push_back(tkn::ValueType::Signed8Bit);
                    else if (identifier == "u16")
                        tokens.push_back(tkn::ValueType::Unsigned16Bit);
                    else if (identifier == "s16")
                        tokens.push_back(tkn::ValueType::Signed16Bit);
                    else if (identifier == "u24")
                        tokens.push_back(tkn::ValueType::Unsigned24Bit);
                    else if (identifier == "s24")
                        tokens.push_back(tkn::ValueType::Signed24Bit);
                    else if (identifier == "u32")
                        tokens.push_back(tkn::ValueType::Unsigned32Bit);
                    else if (identifier == "s32")
                        tokens.push_back(tkn::ValueType::Signed32Bit);
                    else if (identifier == "u48")
                        tokens.push_back(tkn::ValueType::Unsigned48Bit);
                    else if (identifier == "s48")
                        tokens.push_back(tkn::ValueType::Signed48Bit);
                    else if (identifier == "u64")
                        tokens.push_back(tkn::ValueType::Unsigned64Bit);
                    else if (identifier == "s64")
                        tokens.push_back(tkn::ValueType::Signed64Bit);
                    else if (identifier == "u96")
                        tokens.push_back(tkn::ValueType::Unsigned96Bit);
                    else if (identifier == "s96")
                        tokens.push_back(tkn::ValueType::Signed96Bit);
                    else if (identifier == "u128")
                        tokens.push_back(tkn::ValueType::Unsigned128Bit);
                    else if (identifier == "s128")
                        tokens.push_back(tkn::ValueType::Signed128Bit);
                    else if (identifier == "float")
                        tokens.push_back(tkn::ValueType::Float);
                    else if (identifier == "double")
                        tokens.push_back(tkn::ValueType::Double);
                    else if (identifier == "char")
                        tokens.push_back(tkn::ValueType::Character);
                    else if (identifier == "char16")
                        tokens.push_back(tkn::ValueType::Character16);
                    else if (identifier == "bool")
                        tokens.push_back(tkn::ValueType::Boolean);
                    else if (identifier == "str")
                        tokens.push_back(tkn::ValueType::String);
                    else if (identifier == "padding")
                        tokens.push_back(tkn::ValueType::Padding);
                    else if (identifier == "auto")
                        tokens.push_back(tkn::ValueType::Auto);

                    // If it's not a keyword and a builtin type, it has to be an identifier

                    else
                        tokens.emplace_back(tkn::Literal::Identifier(identifier));

                    offset += identifier.length();
                } else if (std::isdigit(c)) {
                    auto integerLength = getIntegerLiteralLength(&code[offset]);
                    auto integer       = lexIntegerLiteralWithSeparator(std::string_view(&code[offset], integerLength));

                    if (!integer.has_value())
                        throwLexerError("invalid integer literal", line);


                    tokens.push_back(tkn::Literal::Numeric(integer.value()));
                    offset += integerLength;
                } else
                    throwLexerError("unknown token", line);

                if (!tokens.empty()) {
                    auto &lastToken = tokens.back();

                    lastToken.line  = line;
                    lastToken.column = offset - lineStartOffset;
                }
            }

            tokens.emplace_back(tkn::Separator::EndOfProgram);
        } catch (PatternLanguageError &e) {
            this->m_error = e;

            return std::nullopt;
        }


        return tokens;
    }
}