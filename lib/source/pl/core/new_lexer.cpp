// lexertl_test.cpp
//

#include <pl/core/new_lexer_sm.hpp>
#include <pl/core/new_lexer.hpp>

#include <pl/api.hpp>
#include <pl/core/token.hpp>
#include <pl/core/tokens.hpp>

#include <lexertl/lookup.hpp>
#include <lexertl/state_machine.hpp>
#include <lexertl/generator.hpp>
// DISABLED: lexertl17 has an issue was all warnings being treated as errors.
//  Submitted issue.
/*#if !defined(DEBUG)
    #include <pl/core/new_lexer_static.hpp>
#endif*/

#include <cstddef>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <fstream>

/*void usegen(const string &input)
{
    lexertl::smatch results(input.begin(), input.end());
    auto lineStart = results.first;
    std::vector<lexertl::smatch::iter_type::difference_type> lengths;

    // Read ahead
    lookup(results);

    while (results.id!=0)
    {
        if (results.id == eNewLine)
        {
            auto len = results.first - lineStart;
            lineStart = results.second;
            lengths.push_back(len);
        }

        if (results.id != lexertl::smatch::npos())
        {
            cout << "Id: " << results.id << ", Token: '" <<
                results.str() << "'\n";
        }

        lookup(results);
    }

    int line = 1;
    for (auto l : lengths) {
        cout << line << ": " << l << endl;
        ++line;
    }
}*/

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
    std::optional<u128> New_Lexer::parseInteger(std::string_view literal, const auto &location) {
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

        return value;
    }

    std::optional<double> New_Lexer::parseFloatingPoint(std::string_view literal, const char suffix, const auto &location) {
        char *end = nullptr;
        double val = std::strtod(literal.data(), &end);

        if(end != literal.data() + literal.size()) {
            error(location(), "Invalid float literal: {}", literal);
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

    std::optional<char> New_Lexer::parseCharacter(const char* &pchar, const auto &location) {
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
                    const char hex[3] = { *pchar, *(pchar+1), 0 }; // TODO: buffer overrun?
                    pchar += sizeof(hex)-1;
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        error(location(), "Invalid hex escape sequence: {}", hex); // TODO: error loc
                        return std::nullopt;
                    }
                }
                case 'u': {
                    const char hex[5] = { *pchar, *(pchar+1), *(pchar+2), *(pchar+3), 0};
                    pchar += sizeof(hex)-1;
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        error(location(), "Invalid unicode escape sequence: {}", hex); // TODO: error loc
                        return std::nullopt;
                    }
                }
                default:
                    error(location(), "Unknown escape sequence: {}", pchar); // TODO: error loc & msg content
                return std::nullopt;
            }
        }
        return c;
    }

    std::optional<Token> New_Lexer::parseStringLiteral(std::string_view literal, const auto &location)
    {
        std::string result;

        const char *p = &literal.front();
        const char *e = &literal.back(); // inclusive
        while (p<=e) {
            auto character = parseCharacter(p, location);
            if (!character.has_value()) {
                return std::nullopt;
            }

            result += character.value();
        }

        (void)location;
        return tkn::Literal::makeString(result); // TODO: location info
    }

    namespace {

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

        lexertl::state_machine g_sm;

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

        newLexerBuild(g_sm);
    }

    hlp::CompileResult<std::vector<Token>> New_Lexer::lex(const api::Source *source)
    {
        m_tokens.clear();
        m_longestLineLength = 0;

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

        lexertl::lookup(g_sm, results);
        for (;;)
        {
            if (results.id==eEOF)
                break;

            switch (results.id) {
            case eNewLine: {
                    ++line;
                    std::size_t len = results.first - lineStart;
                    m_longestLineLength = std::max(len, m_longestLineLength);
                    lineStart = results.second;
                }
                break;
            case eKWNamedOpTypeConstIdent:
            case eOperator: {
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
            case eSingleLineComment: {
                    const std::string_view comment(results.first+2, results.second);
                    auto ctok = tkn::Literal::makeComment(true, std::string(comment));
                    m_tokens.emplace_back(ctok.type, ctok.value, location());
                }
                break;
            case eSingleLineDocComment: {
                    const std::string_view comment(results.first+3, results.second);
                    auto ctok = tkn::Literal::makeDocComment(false, true, std::string(comment));
                    m_tokens.emplace_back(ctok.type, ctok.value, location());
                }
                break;
            case eMultiLineCommentOpen:
                mlcommentType = MLComment;
                mlcomentStartRaw = results.first;
                mlcomentStart = results.first+2;
                mlcommentLocation = location();
                break;
            case eMultiLineDocCommentOpen:
                mlcommentType = (results.first[2]=='*') ? MLLocalDocComment : MLGlobalDocComment;
                mlcomentStartRaw = results.first;
                mlcomentStart = results.first+3;
                mlcommentLocation = location();
                break;
            case eMultiLineCommentClose: {
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

            case eInteger: {
                    const std::string_view numStr(results.first, results.second);
                    auto optNum = parseInteger(numStr, location);
                    if (!optNum.has_value()) {
                        continue;
                    }
                    auto ntok = tkn::Literal::makeNumeric(optNum.value());
                    m_tokens.emplace_back(ntok.type, ntok.value, location());
                }
                break;

            case eFPNumber: {
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

            case eString: {
                    const std::string_view str(results.first+1, results.second-1);
                    auto stok = parseStringLiteral(str, location).value();
                    // TODO: error handling
                    m_tokens.emplace_back(stok.type, stok.value, location());
                    // TODO:
                    //  'makeString' does not take a std::string_view
                    //  Handle string escape sequences
                }
                break;
            case eChar: {
                    const std::string_view ch(results.first+1, results.second-1);
                    const char *p = &ch[0];
                    auto pch = parseCharacter(p, location);
                    // TODO: error handling
                    auto chtok = tkn::Literal::makeNumeric(pch.value());
                    m_tokens.emplace_back(chtok.type, chtok.value, location());
                }
                break;
            case eSeparator: {
                    const char sep = *results.first;
                    const auto separatorToken = Token::Separators().find(sep)->second;
                    m_tokens.emplace_back(separatorToken.type, separatorToken.value, location());
                }
                break;
            case eDirective: {
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
            case eDirectiveType: {
                    const std::string_view type(results.first, results.second);
                    const auto stok = tkn::Literal::makeString(std::string(type));
                    m_tokens.emplace_back(stok.type, stok.value, location());
                }

                break;
            case eDirectiveParam: {
                    const std::string_view param(results.first, results.second);
                    const auto stok = tkn::Literal::makeString(std::string(param));
                    m_tokens.emplace_back(stok.type, stok.value, location());
                }
                break;
            }

            lexertl::lookup(g_sm, results);
        }

        const auto &eop = tkn::Separator::EndOfProgram;
        m_tokens.emplace_back(eop.type, eop.value, location());

        return { m_tokens, collectErrors() };
    }

    void saveCompileResults(std::string fn, hlp::CompileResult<std::vector<Token>> &res)
    {
        auto now = std::chrono::steady_clock::now();
        auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

        std::ofstream ofs(fn+"("+std::to_string(ticks)+").txt");
        const auto &tokens = res.unwrap();
        for (const auto &tok : tokens) {
            const auto &loc = tok.location;
            auto type = tok.getFormattedType();
            auto value = tok.getFormattedValue();
            ofs
                << "type: " << type << ", value: '" << value << "'"
                << " (" << loc.source->source << ", " << loc.line << ":" << loc.column << ")"
                << std::endl;

            int a = 1+1; (void)a;
        }
    }

} // namespace pl::core
