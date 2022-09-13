#include <pl/core/lexer.hpp>

#include <algorithm>
#include <charconv>
#include <functional>
#include <optional>
#include <vector>

namespace pl::core {

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
                bool validChar = [c, base]{
                    if (base == 10)
                        return c >= '0' && c <= '9';
                    else if (base == 2)
                        return c >= '0' && c <= '1';
                    else if (base == 8)
                        return c >= '0' && c <= '7';
                    else if (base == 16)
                        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                    else
                        return false;
                }();

                if (!validChar)
                    err::L0003.throwError(fmt::format("Invalid character '{}' found in base {} integer literal", c, base));

                value *= base;
                value += [&] {
                    if (c >= '0' && c <= '9') return c - '0';
                    else if (c >= 'A' && c <= 'F') return 0xA + (c - 'A');
                    else if (c >= 'a' && c <= 'f') return 0xA + (c - 'a');
                    else
                        err::L0003.throwError("Invalid integer sequence.");
                }();
            }

            if (isUnsigned)
                return value;
            else
                return i128(value);
        }

        err::L0003.throwError("Invalid sequence");
    }

    std::optional<Token::Literal> lexIntegerLiteralWithSeparator(std::string_view string) {

        if (string.starts_with('\'') || string.ends_with('\''))
            err::L0003.throwError("Integer literal cannot start or end with separator character '");
        else if (string.find('\'') == std::string_view::npos)
            return lexIntegerLiteral(string);
        else {
            auto preprocessedString = std::string(string);
            preprocessedString.erase(std::remove(preprocessedString.begin(), preprocessedString.end(), '\''), preprocessedString.end());
            return lexIntegerLiteral(preprocessedString);
        }
    }

    std::optional<std::pair<char, size_t>> getCharacter(std::string_view string) {

        if (string.length() < 1)
            err::L0001.throwError("Expected character");

        // Escape sequences
        if (string[0] == '\\') {

            if (string.length() < 2)
                err::L0001.throwError("Invalid escape sequence");

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

            err::L0001.throwError(fmt::format("Unknown escape sequence '\\{}'", string[1]));
        } else return {
            {string[0], 1}
 };
    }

    std::optional<std::pair<std::string, size_t>> getStringLiteral(std::string_view string) {
        if (!string.starts_with('\"'))
            err::L0002.throwError(fmt::format("Expected opening \" character, got {}", string[0]));

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
                err::L0002.throwError("Expected closing \" before end of input");
        }

        return {
            {result, size + 1}
        };
    }

    std::optional<std::pair<char, size_t>> getCharacterLiteral(std::string_view string) {
        if (string.empty())
            err::L0001.throwError("Reached end of input");

        if (string[0] != '\'')
            err::L0001.throwError(fmt::format("Expected opening ', got {}", string[0]));


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

    std::optional<std::vector<Token>> Lexer::lex(const std::string &sourceCode, const std::string &preprocessedSourceCode) {
        std::vector<Token> tokens;
        u32 offset = 0;

        u32 line = 1;
        u32 lineStartOffset = 0;

        const auto addToken = [&](Token token) {
            token.line = line;
            token.column = (offset - lineStartOffset) + 1;
            
            tokens.emplace_back(std::move(token));
        };
        
        const std::string_view code = preprocessedSourceCode;

        try {

            while (offset < preprocessedSourceCode.length()) {
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
                    addToken(tkn::Separator::Semicolon);
                    offset += 1;
                } else if (c == '(') {
                    addToken(tkn::Separator::LeftParenthesis);
                    offset += 1;
                } else if (c == ')') {
                    addToken(tkn::Separator::RightParenthesis);
                    offset += 1;
                } else if (c == '{') {
                    addToken(tkn::Separator::LeftBrace);
                    offset += 1;
                } else if (c == '}') {
                    addToken(tkn::Separator::RightBrace);
                    offset += 1;
                } else if (c == '[') {
                    addToken(tkn::Separator::LeftBracket);
                    offset += 1;
                } else if (c == ']') {
                    addToken(tkn::Separator::RightBracket);
                    offset += 1;
                } else if (c == ',') {
                    addToken(tkn::Separator::Comma);
                    offset += 1;
                } else if (c == '.') {
                    addToken(tkn::Separator::Dot);
                    offset += 1;
                } else if (code.substr(offset, 2) == "::") {
                    addToken(tkn::Operator::ScopeResolution);
                    offset += 2;
                } else if (c == '@') {
                    addToken(tkn::Operator::At);
                    offset += 1;
                } else if (code.substr(offset, 2) == "==") {
                    addToken(tkn::Operator::BoolEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == "!=") {
                    addToken(tkn::Operator::BoolNotEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == ">=") {
                    addToken(tkn::Operator::BoolGreaterThanOrEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == "<=") {
                    addToken(tkn::Operator::BoolLessThanOrEqual);
                    offset += 2;
                } else if (code.substr(offset, 2) == "&&") {
                    addToken(tkn::Operator::BoolAnd);
                    offset += 2;
                } else if (code.substr(offset, 2) == "||") {
                    addToken(tkn::Operator::BoolOr);
                    offset += 2;
                } else if (code.substr(offset, 2) == "^^") {
                    addToken(tkn::Operator::BoolXor);
                    offset += 2;
                } else if (c == '=') {
                    addToken(tkn::Operator::Assign);
                    offset += 1;
                } else if (c == ':') {
                    addToken(tkn::Operator::Colon);
                    offset += 1;
                } else if (c == '+') {
                    addToken(tkn::Operator::Plus);
                    offset += 1;
                } else if (c == '-') {
                    addToken(tkn::Operator::Minus);
                    offset += 1;
                } else if (c == '*') {
                    addToken(tkn::Operator::Star);
                    offset += 1;
                } else if (c == '/') {
                    addToken(tkn::Operator::Slash);
                    offset += 1;
                } else if (c == '%') {
                    addToken(tkn::Operator::Percent);
                    offset += 1;
                } else if (code.substr(offset, 2) == "<<") {
                    addToken(tkn::Operator::LeftShift);
                    offset += 2;
                } else if (code.substr(offset, 2) == ">>") {
                    addToken(tkn::Operator::RightShift);
                    offset += 2;
                } else if (c == '>') {
                    addToken(tkn::Operator::BoolGreaterThan);
                    offset += 1;
                } else if (c == '<') {
                    addToken(tkn::Operator::BoolLessThan);
                    offset += 1;
                } else if (c == '!') {
                    addToken(tkn::Operator::BoolNot);
                    offset += 1;
                } else if (c == '|') {
                    addToken(tkn::Operator::BitOr);
                    offset += 1;
                } else if (c == '&') {
                    addToken(tkn::Operator::BitAnd);
                    offset += 1;
                } else if (c == '^') {
                    addToken(tkn::Operator::BitXor);
                    offset += 1;
                } else if (c == '~') {
                    addToken(tkn::Operator::BitNot);
                    offset += 1;
                } else if (c == '?') {
                    addToken(tkn::Operator::TernaryConditional);
                    offset += 1;
                } else if (c == '$') {
                    addToken(tkn::Operator::Dollar);
                    offset += 1;
                } else if (code.substr(offset, 9) == "addressof" && !isIdentifierCharacter(code[offset + 9])) {
                    addToken(tkn::Operator::AddressOf);
                    offset += 9;
                } else if (code.substr(offset, 6) == "sizeof" && !isIdentifierCharacter(code[offset + 6])) {
                    addToken(tkn::Operator::SizeOf);
                    offset += 6;
                } else if (c == '\'') {
                    auto lexedCharacter = getCharacterLiteral(code.substr(offset));

                    if (!lexedCharacter.has_value())
                        err::L0001.throwError("Invalid character literal");

                    auto [character, charSize] = lexedCharacter.value();

                    addToken(tkn::Literal::NumericValue(character));
                    offset += charSize;
                } else if (c == '\"') {
                    auto string = getStringLiteral(code.substr(offset));

                    if (!string.has_value())
                        err::L0002.throwError("Invalid string literal");

                    auto [s, stringSize] = string.value();

                    addToken(tkn::Literal::StringValue(s));
                    offset += stringSize;
                } else if (isIdentifierCharacter(c) && !std::isdigit(c)) {
                    std::string identifier = matchTillInvalid(&code[offset], isIdentifierCharacter);

                    // Check for reserved keywords

                    if (identifier == "struct")
                        addToken(tkn::Keyword::Struct);
                    else if (identifier == "union")
                        addToken(tkn::Keyword::Union);
                    else if (identifier == "using")
                        addToken(tkn::Keyword::Using);
                    else if (identifier == "enum")
                        addToken(tkn::Keyword::Enum);
                    else if (identifier == "bitfield")
                        addToken(tkn::Keyword::Bitfield);
                    else if (identifier == "be")
                        addToken(tkn::Keyword::BigEndian);
                    else if (identifier == "le")
                        addToken(tkn::Keyword::LittleEndian);
                    else if (identifier == "if")
                        addToken(tkn::Keyword::If);
                    else if (identifier == "else")
                        addToken(tkn::Keyword::Else);
                    else if (identifier == "false")
                        addToken(tkn::Literal::NumericValue(false));
                    else if (identifier == "true")
                        addToken(tkn::Literal::NumericValue(true));
                    else if (identifier == "parent")
                        addToken(tkn::Keyword::Parent);
                    else if (identifier == "this")
                        addToken(tkn::Keyword::This);
                    else if (identifier == "while")
                        addToken(tkn::Keyword::While);
                    else if (identifier == "for")
                        addToken(tkn::Keyword::For);
                    else if (identifier == "fn")
                        addToken(tkn::Keyword::Function);
                    else if (identifier == "return")
                        addToken(tkn::Keyword::Return);
                    else if (identifier == "namespace")
                        addToken(tkn::Keyword::Namespace);
                    else if (identifier == "in")
                        addToken(tkn::Keyword::In);
                    else if (identifier == "out")
                        addToken(tkn::Keyword::Out);
                    else if (identifier == "break")
                        addToken(tkn::Keyword::Break);
                    else if (identifier == "continue")
                        addToken(tkn::Keyword::Continue);
                    else if (identifier == "ref")
                        addToken(tkn::Keyword::Reference);

                    // Check for built-in types
                    else if (identifier == "u8")
                        addToken(tkn::ValueType::Unsigned8Bit);
                    else if (identifier == "s8")
                        addToken(tkn::ValueType::Signed8Bit);
                    else if (identifier == "u16")
                        addToken(tkn::ValueType::Unsigned16Bit);
                    else if (identifier == "s16")
                        addToken(tkn::ValueType::Signed16Bit);
                    else if (identifier == "u24")
                        addToken(tkn::ValueType::Unsigned24Bit);
                    else if (identifier == "s24")
                        addToken(tkn::ValueType::Signed24Bit);
                    else if (identifier == "u32")
                        addToken(tkn::ValueType::Unsigned32Bit);
                    else if (identifier == "s32")
                        addToken(tkn::ValueType::Signed32Bit);
                    else if (identifier == "u48")
                        addToken(tkn::ValueType::Unsigned48Bit);
                    else if (identifier == "s48")
                        addToken(tkn::ValueType::Signed48Bit);
                    else if (identifier == "u64")
                        addToken(tkn::ValueType::Unsigned64Bit);
                    else if (identifier == "s64")
                        addToken(tkn::ValueType::Signed64Bit);
                    else if (identifier == "u96")
                        addToken(tkn::ValueType::Unsigned96Bit);
                    else if (identifier == "s96")
                        addToken(tkn::ValueType::Signed96Bit);
                    else if (identifier == "u128")
                        addToken(tkn::ValueType::Unsigned128Bit);
                    else if (identifier == "s128")
                        addToken(tkn::ValueType::Signed128Bit);
                    else if (identifier == "float")
                        addToken(tkn::ValueType::Float);
                    else if (identifier == "double")
                        addToken(tkn::ValueType::Double);
                    else if (identifier == "char")
                        addToken(tkn::ValueType::Character);
                    else if (identifier == "char16")
                        addToken(tkn::ValueType::Character16);
                    else if (identifier == "bool")
                        addToken(tkn::ValueType::Boolean);
                    else if (identifier == "str")
                        addToken(tkn::ValueType::String);
                    else if (identifier == "padding")
                        addToken(tkn::ValueType::Padding);
                    else if (identifier == "auto")
                        addToken(tkn::ValueType::Auto);

                    // If it's not a keyword and a builtin type, it has to be an identifier

                    else
                        addToken(tkn::Literal::IdentifierValue(identifier));

                    offset += identifier.length();
                } else if (std::isdigit(c)) {
                    auto integerLength = getIntegerLiteralLength(&code[offset]);
                    auto integer       = lexIntegerLiteralWithSeparator(std::string_view(&code[offset], integerLength));

                    if (!integer.has_value())
                        err::L0003.throwError("Invalid integer literal");


                    addToken(tkn::Literal::NumericValue(integer.value()));
                    offset += integerLength;
                } else
                    err::L0004.throwError("Unknown sequence");
            }

            addToken(tkn::Separator::EndOfProgram);
        } catch (err::LexerError::Exception &e) {
            auto column = (offset - lineStartOffset) + 1;
            this->m_error = err::PatternLanguageError(e.format(sourceCode, line, column), line, column);

            return std::nullopt;
        }


        return tokens;
    }
}