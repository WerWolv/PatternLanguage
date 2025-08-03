// lexertl_test.cpp
//

#include <pl/core/new_lexer.hpp>

#include <pl/api.hpp>
#include <pl/core/token.hpp>
#include <pl/core/tokens.hpp>

//#include "out.cpp"

#include <lexertl/lookup.hpp>
#include <lexertl/state_machine.hpp>
#include <lexertl/generator.hpp>
#include <lexertl/generate_cpp.hpp>
#include <pl/helpers/utils.hpp>

#include <cstddef>
#include <cctype>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <sstream>

// Debugging
#include <iostream>
#include <fstream>
#include <chrono>
using std::cout;
using std::endl;

using std::string;
using std::string_view;

// void compile(lexertl::state_machine &sm, lexertl::rules &rules)
// {
//     rules.push_state("MLCOMMENT");
//     rules.push_state("PRAGMA1");
//     rules.push_state("PRAGMA2");

//     rules.push("*", "\n", eNewLine, ".");

//     rules.push(R"(\*\*\*)", eBreakPoint);

//     rules.push("INITIAL", "\"/*\"", eMultiLineComment, "MLCOMMENT");
//     rules.push("MLCOMMENT", R"([^*\n]+|.)", eMultiLineComment, ".");
//     rules.push("MLCOMMENT", "\"*/\"", eMultiLineComment, "INITIAL");

//     rules.push(rx_strings(g_keywords), eKeyword);
//     rules.push(rx_strings(g_namedOperators), eNamedOperator);
//     rules.push(rx_strings(g_types), eType);
//     rules.push(rx_strings(g_constants), eConstant);
//     rules.push(R"([_a-zA-Z][_a-zA-Z0-9]*)", eIdentifier);
//     rules.push(
//         R"((?:0[xX][0-9a-fA-F]+|0[bB][01]+|0[0-7]*|\d+\.\d*([eE][+-]?\d+)?|\.\d+([eE][+-]?\d+)?|\d+[eE][+-]?\d+|\d+)(?:[uU]?[lL]{0,2}|[lL]{1,2}[uU]?)?[fFlL]?)",
//         eNumber
//     );
//     rules.push(R"(\/\/.*$)", eSingleLineComment);
//     rules.push(rx_esc_strings(g_operators), eOperator);
//     rules.push(rx_esc_strings(g_seperators), eSeperator);
//     rules.push("INITIAL", R"(#\s*pragma)", ePragma, "PRAGMA1");
//     rules.push("PRAGMA1", R"([_a-zA-Z][_a-zA-Z0-9]*)", ePragmaType, "PRAGMA2");
//     rules.push("PRAGMA2", R"([a-zA-Z].*)", ePragmaParam, "INITIAL");
//     rules.push(rx_directives(), eDirective);

//     rules.push(R"(["]([^"\\\n]|\\.|\\\n)*["])", eString);

//     lexertl::generator::build(rules, sm);
// }

// string dynamic(const string& input)
// {
//     std::ostringstream oss;

//     lexertl::state_machine sm;
//     lexertl::rules rules;
//     compile(sm, rules);

//     lexertl::smatch results(input.begin(), input.end());
//     //auto line_start = results.first;
//     //std::vector<lexertl::smatch::iter_type::difference_type> lengths;

//     // Read ahead
//     lexertl::lookup(sm, results);

//     while (results.id!=0)
//     {
//         /*if (results.id == eNewLine)
//         {
//             auto len = results.first - line_start;
//             line_start = results.second;
//             lengths.push_back(len);
//         }*/

//         if (results.id != lexertl::smatch::npos())
//         {
//             oss << "Id: " << results.id << ", Token: '" <<
//                 results.str() << "'\n";
//         }

//         if (results.id == eBreakPoint)
//         {
//             int a = 0; (void)a;
//         }

//         lexertl::lookup(sm, results);
//     }

//     /*int line = 1;
//     for (auto l : lengths) {
//         oss << line << ": " << l << endl;
//         ++line;
//     }*/

//     return oss.str();
// }

/*void generate()
{
    lexertl::state_machine sm;
    lexertl::rules rules;
    compile(sm, rules);

    sm.minimise();
    std::ofstream ofs("out.cpp");
    lexertl::table_based_cpp::generate_cpp("lookup", sm, false, ofs);
}

void usegen(const string &input)
{
    lexertl::smatch results(input.begin(), input.end());
    auto line_start = results.first;
    std::vector<lexertl::smatch::iter_type::difference_type> lengths;

    // Read ahead
    lookup(results);

    while (results.id!=0)
    {
        if (results.id == eNewLine)
        {
            auto len = results.first - line_start;
            line_start = results.second;
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

    std::optional<Token::Literal> New_Lexer::parseNumericLiteral(std::string_view literal, const auto &location) {
        // parse a c like numeric literal
        const bool floatSuffix = hlp::stringEndsWithOneOf(literal, { "f", "F", "d", "D" });
        const bool unsignedSuffix = hlp::stringEndsWithOneOf(literal, { "u", "U" });
        const bool isFloat = literal.find('.') != std::string_view::npos
                       || (!literal.starts_with("0x") && floatSuffix);

        if(isFloat) {
            char suffix = 0;
            if(floatSuffix) {
                // remove suffix
                suffix = literal.back();
                literal = literal.substr(0, literal.size() - 1);
            }

            auto floatingPoint = parseFloatingPoint(literal, suffix, location);

            if(!floatingPoint.has_value()) return std::nullopt;

            return floatingPoint.value();
        }

        if(unsignedSuffix) {
            // remove suffix
            literal = literal.substr(0, literal.size() - 1);
        }

        const auto integer = parseInteger(literal, location);

        if(!integer.has_value()) return std::nullopt;

        u128 value = integer.value();
        if(unsignedSuffix) {
            return value;
        }

        return i128(value);
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
                    const char hex[3] = { *pchar, *pchar++, 0 }; // TODO: buffer overrun?
                    try {
                        return static_cast<char>(std::stoul(hex, nullptr, 16));
                    } catch (const std::invalid_argument&) {
                        error(location(), "Invalid hex escape sequence: {}", hex); // TODO: error loc
                        return std::nullopt;
                    }
                }
                case 'u': {
                    const char hex[5] = { *pchar, *pchar++, *pchar++, *pchar++, 0};
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

            std::size_t operator()(const string &s) const noexcept {
                return std::hash<string>{}(s);
            }

            std::size_t operator()(string_view s) const noexcept {
                return std::hash<string_view>{}(s);
            }
        };

        struct TransEqual {
            using is_transparent = void;

            bool operator()(const string &lhs, const string &rhs) const noexcept {
                return lhs == rhs;
            }

            bool operator()(const string &lhs, string_view rhs) const noexcept {
                return lhs == rhs;
            }

            bool operator()(string_view lhs, const string& rhs) const noexcept {
                return lhs == rhs;
            }

            bool operator()(string_view lhs, string_view rhs) const noexcept {
                return lhs == rhs;
            }
        };

        std::unordered_map<std::string, KWOpTypeInfo, TransHash, TransEqual> g_KWOpTypeTokenInfo;

        lexertl::state_machine g_sm;

        inline bool must_escape(char c)
        {
            switch (c) {
            case '+': case '-': case '/': case '*': case '?':
            case '|':
            case '(': case ')':
            case '[': case ']':
            case '{': case '}':
            case '.':
            case '^': case '$':
            case '\\':
            case '"':
                return true;

            default:
                break;
            }
            return false;
        }

        template <typename String>
        inline string escape_regex(const String& s) {
            string result;
            result.reserve(s.size() * 2);

            for (char c : s) {
                if (must_escape(c))
                    result += '\\';
                result += c;
            }

            return result;
        }

        inline string escape_regex(const char *s) {
            return escape_regex(std::string(s));
        }

        enum {
            eEOF, eNewLine, eKWNamedOpTypeConstIdent,
            eSingleLineComment, eSingleLineDocComment,
            eMultiLineCommentOpen, eMultiLineDocCommentOpen, eMultiLineCommentClose,
            eNumber, eString, eSeparator, eDirective, eDirectiveType, eDirectiveParam,
            eOperator, eChar
        };

    } // anonymous namespace

    void init_new_lexer()
    {
        lexertl::rules rules;

        rules.push_state("MLCOMMENT");
        rules.push_state("DIRECTIVETYPE");
        rules.push_state("DIRECTIVEPARAM");

        rules.push("\r\n|\n|\r", eNewLine);

        rules.push(R"(\/\/[^/][^\r\n]*)", eSingleLineComment);
        rules.push(R"(\/\/\/[^\r\n]*)", eSingleLineDocComment);

        rules.push("INITIAL", R"(\/\*[^*!\r\n].*)", eMultiLineCommentOpen, "MLCOMMENT");
        rules.push("INITIAL", R"(\/\*[*!].*)", eMultiLineDocCommentOpen, "MLCOMMENT");
        rules.push("MLCOMMENT", "\r\n|\n|\r", eNewLine, ".");
        rules.push("MLCOMMENT", R"([^*\r\n]+|.)", lexertl::rules::skip(), "MLCOMMENT");
        rules.push("MLCOMMENT", R"(\*\/)", eMultiLineCommentClose, "INITIAL");

        rules.push(R"([a-zA-Z_]\w*)", eKWNamedOpTypeConstIdent);

        rules.push(R"([0-9][0-9a-fA-F'xXoOpP.uU+-]*)", eNumber);

        rules.push(R"(["](\\.|[^"\\])*["])", eString); // TODO: improve string handling
        rules.push(R"('.*')", eChar); // TODO: fix this

        rules.push("INITIAL", R"(#\s*[a-zA-Z_]\w*)", eDirective, "DIRECTIVETYPE");
        rules.push("DIRECTIVETYPE", R"([_a-zA-Z][_a-zA-Z0-9]*)", eDirectiveType, "DIRECTIVEPARAM");
        rules.push("DIRECTIVEPARAM", "\r\n|\n|\r", eNewLine, "INITIAL");
        rules.push("DIRECTIVEPARAM", R"(\S.*)", eDirectiveParam, "INITIAL");

        // The parser expects >= and <= as two serqrate tokens. Not sure why.
        // I origionally intended to handle this differently but there are
        // many issues similar to what I just desribed which make this difficult.
        // I may address this in future if this code every sees daylight.
        const char* ops[] = {"+", "-", "*", "/", "%", "&", "|", "^", "~", "==", "!=", "<", ">",
                             "&&", "||", "!", "^^", "$", ":", "::", "?", "@", "=", "addressof",
                             "sizeof", "typenameof"};
        std::ostringstream ops_ss; 
        for (auto op : ops) {
            ops_ss << escape_regex(op) << "|";
        }
        string oprs = ops_ss.str();
        oprs.pop_back();
        rules.push(oprs, eOperator);

        string sep_chars = escape_regex("(){}[],.;");
        rules.push("["+sep_chars+"]", eSeparator);

        const auto &keywords = Token::Keywords();
        for (const auto& [key, value] : keywords)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));

        //std::ostringstream ops_ss;
        const auto &operators = Token::Operators();
        for (const auto& [key, value] : operators) {
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));
            //ops_ss << escape_regex(key) << "|";
        }
        //string ops = ops_ss.str();
        //ops.pop_back();
        //rules.push(ops, eOperator);

        const auto &types = Token::Types();
        for (const auto& [key, value] : types)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));

        /*std::ostringstream sep_ss;
        const auto &seps = Token::Separators();
        for (const auto& [key, value] : seps) {
            string name(1, key);
            name = escape_regex(name);
            sep_ss << name << "|";
        }
        string sep = sep_ss.str();
        sep.pop_back();
        rules.push(sep, eSeparator);*/

        lexertl::generator::build(rules, g_sm);
    }

    hlp::CompileResult<std::vector<Token>> New_Lexer::lex(const api::Source *source)
    {
        m_tokens.clear();
        m_longestLineLength = 0;

        string::const_iterator content_end = source->content.end();
        lexertl::smatch results(source->content.begin(), content_end);

        auto line_start = results.first;
        u32 line = 1;

        auto location = [&]() -> Location {
            u32 column = results.first-line_start+1;
            size_t errorLength = results.second-results.first;
            return Location { source, line, column, errorLength };
        };

        string::const_iterator mlcoment_start_raw; // start of parsed token, no skipping
        string::const_iterator mlcoment_start;
        Location mlcomment_location;

        enum MLCommentType{
            MLComment,
            MLLocalDocComment,
            MLGlobalDocComment
        };

        MLCommentType mlcomment_type;

        lexertl::lookup(g_sm, results);
        for (;;)
        {
            if (results.id==eEOF)
                break;

            switch (results.id) {
            case eNewLine: {
                    ++line;
                    std::size_t len = results.first - line_start;
                    m_longestLineLength = std::max(len, m_longestLineLength);
                    line_start = results.second;
                }
                break;
            case eKWNamedOpTypeConstIdent:
            case eOperator: {
                    const string_view kw(results.first, results.second);
                    if (const auto it = g_KWOpTypeTokenInfo.find(kw); it != g_KWOpTypeTokenInfo.end()) {
                        m_tokens.emplace_back(it->second.type, it->second.value, location());
                    }
                    else if (const auto it = tkn::constants.find(kw); it != tkn::constants.end()) {
                        auto ctok = tkn::Literal::makeNumeric(it->second);
                        m_tokens.emplace_back(ctok.type, ctok.value, location());
                    }
                    else {
                        auto idtok = tkn::Literal::makeIdentifier(string(kw));
                        m_tokens.emplace_back(idtok.type, idtok.value, location());
                    }
                }
                break;
            case eSingleLineComment: {
                    const string_view comment(results.first+2, results.second);
                    auto ctok = tkn::Literal::makeComment(true, string(comment));
                    m_tokens.emplace_back(ctok.type, ctok.value, location());
                }
                break;
            case eSingleLineDocComment: {
                    const string_view comment(results.first+3, results.second);
                    auto ctok = tkn::Literal::makeDocComment(false, true, string(comment));
                    m_tokens.emplace_back(ctok.type, ctok.value, location());
                }
                break;
            case eMultiLineCommentOpen:
                mlcomment_type = MLComment;
                mlcoment_start_raw = results.first;
                mlcoment_start = results.first+2;
                mlcomment_location = location();
                break;
            case eMultiLineDocCommentOpen:
                mlcomment_type = (results.first[2]=='*') ? MLLocalDocComment : MLGlobalDocComment;
                mlcoment_start_raw = results.first;
                mlcoment_start = results.first+3;
                mlcomment_location = location();
                break;
            case eMultiLineCommentClose: {
                    mlcomment_location.length = results.second-mlcoment_start_raw;
                    const string_view comment(mlcoment_start, results.second-2);

                    switch (mlcomment_type) {
                    case MLComment: {
                            auto ctok = tkn::Literal::makeComment(false, string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcomment_location);
                        }
                        break;
                    case MLLocalDocComment: {
                            auto ctok = tkn::Literal::makeDocComment(false, false, string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcomment_location);
                        }
                        break;
                    case MLGlobalDocComment: {
                            auto ctok = tkn::Literal::makeDocComment(true, false, string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcomment_location);
                        }
                        break;
                    }
                }
                break;
            case eNumber: {
                    const string_view num_str(results.first, results.second);
                    const auto num = parseNumericLiteral(num_str, location);
                    if (num.has_value()) {
                        auto ntok = tkn::Literal::makeNumeric(num.value());
                        m_tokens.emplace_back(ntok.type, ntok.value, location());
                    }
                }
                break;
            case eString: {
                    const string_view str(results.first+1, results.second-1);
                    auto stok = parseStringLiteral(str, location).value();
                    // TODO: error handling
                    m_tokens.emplace_back(stok.type, stok.value, location());
                    // TODO:
                    //  'makeString' does not take a std::string_view
                    //  Handle string escape sequences
                }
                break;
            case eChar: {
                    const string_view ch(results.first+1, results.second-1);
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
                    string name = "#"+string(first, results.second); // TODO: I don't like this!
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
                    const string_view type(results.first, results.second);
                    const auto stok = tkn::Literal::makeString(string(type));
                    m_tokens.emplace_back(stok.type, stok.value, location());
                }

                break;
            case eDirectiveParam: {
                    const string_view param(results.first, results.second);
                    const auto stok = tkn::Literal::makeString(string(param));
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

    // Debugging
    [[maybe_unused]] void save_compile_results(string fn, hlp::CompileResult<std::vector<Token>> &res)
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
                << endl;

            int a = 1+1; (void)a;
        }
    }

} // namespace pl::core

/*
Notes:
 The code that handles idefs and defines is confusing. The identifier (what is defined)
 is is not handled in the directive's code path (in most other constructs this is the case)
 but in the subsequent iterations of lexing the loop.
*/
