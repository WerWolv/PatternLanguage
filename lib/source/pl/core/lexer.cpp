// new_lexer.cpp
//

#include <pl/core/lexer_sm.hpp>
#include <pl/core/lexer.hpp>

#include <pl/api.hpp>
#include <pl/core/token.hpp>
#include <pl/core/tokens.hpp>
#include <wolv/utils/charconv.hpp>

#include <lexertl/lookup.hpp>
#include <lexertl/state_machine.hpp>
#include <lexertl/generator.hpp>

#if defined(NDEBUG)
    #include <pl/core/generated/lexer_static.hpp>
#endif

#include <cstddef>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace pl::core {

    // TODO:
    //  "integerSeparator_" is defined in "lexer.cpp".
    //  Remove trailing underscore when it's removed.
    static constexpr char integerSeparator_ = '\'';

    // TODO:
    //  "characterValue" is defined in "lexer.cpp".
    //  Remove trailing underscore when it's removed.
    static int characterValue_(const char c) {
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

    // TODO:
    //  "isIntegerCharacter" is defined in "lexer.cpp".
    //  Remove trailing underscore when it's removed.
    static bool isIntegerCharacter_(const char c, const int base) {
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

    // TODO:
    //  Consider making 'location' not use templates. Here and in other functions below.
    std::optional<Token::Literal> Lexer::parseInteger(std::string_view literal, const auto &location) {
        const bool isUnsigned = hlp::stringEndsWithOneOf(literal, { "u", "U" });
        if(isUnsigned) {
            // remove suffix
            literal = literal.substr(0, literal.size() - 1);
        }

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
            if(c == integerSeparator_) continue;

            if (!isIntegerCharacter_(c, base)) {
                error(location(), "Invalid integer literal: {}", literal);
                return std::nullopt;
            }
            value = value * base + characterValue_(c);
        }

        if (isUnsigned) {
            return value;
        }

        return i128(value);
    }

    std::optional<double> Lexer::parseFloatingPoint(std::string_view literal, const char suffix, const auto &location) {
        const auto value = wolv::util::from_chars<double>(literal);

        if(!value.has_value()) {
            error(location(), "Invalid float literal: {}", literal);
            return std::nullopt;
        }

        switch (suffix) {
            case 'f':
            case 'F':
                return float(*value);
            case 'd':
            case 'D':
            default:
                return *value;
        }
    }

    std::optional<char> Lexer::parseCharacter(const char* &pchar, const char* e, const auto &location) {
        const char c = *(pchar++);
        if (c == '\\') {
            switch (*(pchar++)) {
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
                    if (pchar+1 >= e) {
                        error(location(), "Incomplete escape sequence");
                        return std::nullopt;
                    }
                    const char hex[3] = { *pchar, *(pchar+1), 0 }; // TODO: buffer overrun?
                    pchar += sizeof(hex)-1;
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        error(location(), "Invalid hex escape sequence: {}", hex);
                        return std::nullopt;
                    }
                }
                case 'u': {
                     if (pchar+3 >= e) {
                        error(location(), "Incomplete escape sequence");
                        return std::nullopt;
                    }
                    const char hex[5] = { *pchar, *(pchar+1), *(pchar+2), *(pchar+3), 0};
                    pchar += sizeof(hex)-1;
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        error(location(), "Invalid unicode escape sequence: {}", hex);
                        return std::nullopt;
                    }
                }
                default:
                    error(location(), "Unknown escape sequence: {}", pchar);
                return std::nullopt;
            }
        }
        return c;
    }

    std::optional<Token> Lexer::parseStringLiteral(std::string_view literal, const auto &location)
    {
        std::string result;

        if (literal.size()==0) {
            return Token{Token::Type::String, Token::Literal(result), location()};
        }

        const char *p = &literal.front();
        const char *e = &literal.back(); // inclusive
        while (p<=e) {
            auto character = parseCharacter(p, e+1, location);
            if (!character.has_value()) {
                return std::nullopt;
            }

            result += character.value();
        }

        return Token{Token::Type::String, Token::Literal(result), location()};
    }

    namespace {
        bool g_lexerStaticInitDone = false;

        // Much of the contents of this anonymous namespace serve as conceptually
        // private static members of the Lexer class. They're placed here to avoid
        // pulling in unnecessary symbols into every file that includes our header.

        struct KWOpTypeInfo {
            Token::Type type;
            Token::ValueTypes value;
        };

        // This "Trans" stuff is to allow us to use std::string_view to lookup stuff
        // so we don't have to construct a std::string.
        struct TransHash {
            using is_transparent = void;

            std::size_t operator()(const std::string &s) const noexcept {
                return std::hash<std::string>{}(s);
            }

            std::size_t operator()(std::string_view s) const noexcept {
                return std::hash<std::string_view>{}(s);
            }
        };

        struct TransEqual {
            using is_transparent = void;

            bool operator()(const std::string &lhs, const std::string &rhs) const noexcept {
                return lhs == rhs;
            }

            bool operator()(const std::string &lhs, std::string_view rhs) const noexcept {
                return lhs == rhs;
            }

            bool operator()(std::string_view lhs, const std::string& rhs) const noexcept {
                return lhs == rhs;
            }

            bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
                return lhs == rhs;
            }
        };

        std::unordered_map<std::string, KWOpTypeInfo, TransHash, TransEqual> g_KWOpTypeTokenInfo;

#if !defined(NDEBUG)
        lexertl::state_machine g_sm;
#endif

    } // anonymous namespace

    void initNewLexer()
    {
        const auto &keywords = Token::Keywords();
        for (const auto& [key, value] : keywords)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));
    
        const auto &operators = Token::Operators();
        for (const auto& [key, value] : operators) {
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));
        }

        const auto &types = Token::Types();
        for (const auto& [key, value] : types)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));

#if !defined(NDEBUG)
        newLexerBuild(g_sm);
#endif
    }

    Lexer::Lexer() {
        if (!g_lexerStaticInitDone) {
            g_lexerStaticInitDone = true;
            initNewLexer();
        }
        m_longestLineLength = 0;
    }

    void Lexer::reset() {
        m_longestLineLength = 0;
        this->m_tokens.clear();
    }

    hlp::CompileResult<std::vector<Token>> Lexer::lex(const api::Source *source)
    {
        reset();

        std::string::const_iterator contentEnd = source->content.end();
        lexertl::smatch results(source->content.begin(), contentEnd);

        auto lineStart = results.first;
        u32 line = 1;

        auto location = [&]() -> Location {
            u32 column = results.first-lineStart+1;
            size_t errorLength = results.second-results.first;
            return Location { source, line, column, errorLength };
        };

        std::string::const_iterator mlcomentStartRaw; // start of parsed token, no skipping
        std::string::const_iterator mlcomentStart;
        Location mlcommentLocation;

        enum MLCommentType{
            MLComment,
            MLLocalDocComment,
            MLGlobalDocComment
        };

        MLCommentType mlcommentType = MLComment;
#if defined(NDEBUG)
            lookup(results);
#else
            lexertl::lookup(g_sm, results);
#endif
        for (;;)
        {
            if (results.id==LexerToken::EndOfFile)
                break;

            switch (results.id) {
            case (lexertl::smatch::id_type)-1: {
                    error(location(), "Unexpected character: {}", *results.first);
                }
                break;
            case LexerToken::NewLine: {
                    ++line;
                    std::size_t len = results.first - lineStart;
                    m_longestLineLength = std::max(len, m_longestLineLength);
                    lineStart = results.second;
                }
                break;
            case LexerToken::KWNamedOpTypeConstIdent:
            case LexerToken::Operator: {
                    const std::string_view kw(results.first, results.second);

                    if (const auto it = g_KWOpTypeTokenInfo.find(kw); it != g_KWOpTypeTokenInfo.end()) {
                        m_tokens.emplace_back(it->second.type, it->second.value, location());
                    }
                    else if (const auto it = tkn::constants.find(kw); it != tkn::constants.end()) {
                        auto ctok = tkn::Literal::makeNumeric(it->second);
                        m_tokens.emplace_back(ctok.type, ctok.value, location());
                    }
                    else {
                        auto idtok = tkn::Literal::makeIdentifier(std::string(kw));
                        // TODO:
                        //  It seems the presence of a non-zero length in the location info is being
                        //  used by the pattern editor editor for error highlighting. This makes things
                        //  hard. I am trying to include location info in every token. At the very least
                        //  this could make debugging easier. Who knows, it may have other uses. In the
                        //  mean time hack the length to 0.
                        auto loc = location();
                        loc.length = 0;
                        m_tokens.emplace_back(idtok.type, idtok.value, loc);
                    }
                }
                break;
            case LexerToken::SingleLineComment: {
                    const std::string_view comment(results.first+2, results.second);
                    if (comment.size() && comment[0]=='/') {
                        auto ctok = tkn::Literal::makeDocComment(false, true, std::string(comment.substr(1)));
                        m_tokens.emplace_back(ctok.type, ctok.value, location());
                    }
                    else {
                        auto ctok = tkn::Literal::makeComment(true, std::string(comment));
                        m_tokens.emplace_back(ctok.type, ctok.value, location());
                    }
                }
                break;

            case LexerToken::MultiLineCommentOpen: {
                    mlcommentType = MLComment;
                    mlcomentStartRaw = results.first;
                    mlcomentStart = results.first+2;
                    mlcommentLocation = location();

                    const std::string_view comment(results.first+2, results.second);
                    if (comment.size()) {
                        if (comment[0]=='*') {
                            mlcommentType = MLLocalDocComment;
                            ++mlcomentStart;
                        }
                        else if (comment[0]=='!') {
                            mlcommentType = MLGlobalDocComment;
                            ++mlcomentStart;
                        }
                    }
                }
                break;

            case LexerToken::MultiLineCommentClose: {
                    mlcommentLocation.length = results.second-mlcomentStartRaw;
                    const std::string_view comment(mlcomentStart, results.second-2);

                    switch (mlcommentType) {
                    case MLComment: {
                            auto ctok = tkn::Literal::makeComment(false, std::string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcommentLocation);
                        }
                        break;
                    case MLLocalDocComment: {
                            auto ctok = tkn::Literal::makeDocComment(false, false, std::string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcommentLocation);
                        }
                        break;
                    case MLGlobalDocComment: {
                            auto ctok = tkn::Literal::makeDocComment(true, false, std::string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcommentLocation);
                        }
                        break;
                    }
                }
                break;

            case LexerToken::Integer: {
                    const std::string_view numStr(results.first, results.second);
                    auto optNum = parseInteger(numStr, location);
                    if (!optNum.has_value()) {
                        break;
                    }
                    auto ntok = tkn::Literal::makeNumeric(optNum.value());
                    m_tokens.emplace_back(ntok.type, ntok.value, location());
                }
                break;

            case LexerToken::FPNumber: {
                    std::string_view numStr(results.first, results.second);
                    const bool floatSuffix = hlp::stringEndsWithOneOf(numStr, {"f","F","d","D"});
                    char suffix = 0;
                    if (floatSuffix) {
                        // remove suffix
                        suffix = numStr.back();
                        numStr = numStr.substr(0, numStr.size()-1);
                    }
                    auto num = parseFloatingPoint(numStr, suffix, location);
                    if (num.has_value()) {
                        auto ntok = tkn::Literal::makeNumeric(num.value());
                        m_tokens.emplace_back(ntok.type, ntok.value, location());
                    }

                }
                break;

            case LexerToken::String: {
                    const std::string_view str(results.first+1, results.second-1);
                    auto optTok = parseStringLiteral(str, location);
                    if (optTok.has_value()) {
                        auto stok = optTok.value();
                        m_tokens.emplace_back(stok.type, stok.value, location());
                    }
                }
                break;
            case LexerToken::Char: {
                    const std::string_view ch(results.first+1, results.second-1);
                    const char *p = &ch[0];
                    auto optCh = parseCharacter(p, (&ch.back())+1, location);
                    if (optCh.has_value()) {
                        if (p-1 < &ch.back()) {
                            error(location(), "char literal too long");
                        }
                        else if (p-1 > &ch.back()) {
                            error(location(), "char literal too short");
                        }
                        else {
                            m_tokens.emplace_back(core::Token::Type::Integer, optCh.value(), location());
                        }
                    }
                }
                break;
            case LexerToken::Separator: {
                    const char sep = *results.first;
                    const auto separatorToken = Token::Separators().find(sep)->second;
                    m_tokens.emplace_back(separatorToken.type, separatorToken.value, location());
                }
                break;
            case LexerToken::Directive: {
                    auto first = results.first;
                    for (++first; std::isspace(static_cast<unsigned char>(*first)); ++first) {}
                    std::string name = "#"+std::string(first, results.second); // TODO: I don't like this!
                    const auto &directives = Token::Directives();
                    if (const auto directiveToken = directives.find(name); directiveToken != directives.end()) {
                        m_tokens.emplace_back(directiveToken->second.type, directiveToken->second.value, location());
                    }
                    else {
                        error(location(), "Unknown directive: {}", name);
                    }
                }
                break;
            case LexerToken::DirectiveType: {
                    const std::string_view type(results.first, results.second);
                    const auto stok = tkn::Literal::makeString(std::string(type));
                    m_tokens.emplace_back(stok.type, stok.value, location());
                }
                break;
            case LexerToken::DirectiveParam: {
                    const std::string_view param(results.first, results.second);
                    const auto stok = tkn::Literal::makeString(std::string(param));
                    m_tokens.emplace_back(stok.type, stok.value, location());
                }
                break;
            }

#if defined(NDEBUG)
            lookup(results);
#else
            lexertl::lookup(g_sm, results);
#endif
        }

        std::size_t len = results.first - lineStart;
        m_longestLineLength = std::max(len, m_longestLineLength);
        lineStart = results.second;

        const auto &eop = tkn::Separator::EndOfProgram;
        m_tokens.emplace_back(eop.type, eop.value, location());

        return { m_tokens, collectErrors() };
    }

} // namespace pl::core

