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
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

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
        const bool isUnsigned = unsignedSuffix;

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
        if(isUnsigned) {
            return value;
        }

        return i128(value);
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

        enum {
            eEOF, eNewLine, eKWNamedOpTypeConst,
            eSingleLineComment, eSingleLineDocComment,
            eMultiLineCommentOpen, eMultiLineDocCommentOpen, eMultiLineCommentClose,
            eNumber
        };

    } // anonymous namespace

    void init_new_lexer()
    {
        lexertl::rules rules;

        rules.push_state("MLCOMMENT");

        rules.push("*", "\r\n|\n|\r", eNewLine, ".");

        rules.push(R"(\/\/[^/][^\r\n]*)", eSingleLineComment);
        rules.push(R"(\/\/\/[^\r\n]*)", eSingleLineDocComment);

        rules.push("INITIAL", R"(\/\*[^*!\r\n].*)", eMultiLineCommentOpen, "MLCOMMENT");
        rules.push("INITIAL", R"(\/\*[*!].*)", eMultiLineDocCommentOpen, "MLCOMMENT");
        rules.push("MLCOMMENT", R"([^*\r\n]+|.)", lexertl::rules::skip(), "MLCOMMENT");
        rules.push("MLCOMMENT", R"(\*\/)", eMultiLineCommentClose, "INITIAL");

        rules.push(R"([a-zA-Z_]\w*)", eKWNamedOpTypeConst);

        rules.push(R"([0-9][0-9a-fA-F'xXoOpP.uU+-]*)", eNumber);

        auto keywords = Token::Keywords();
        for (const auto& [key, value] : keywords)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value   .type, value.value}));

        auto opeators = Token::Operators();
        for (const auto& [key, value] : opeators)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));

        auto types = Token::Types();
        for (const auto& [key, value] : opeators)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));

        /*for (const auto& [key, value] : pl::core::tkn::constants)
            g_KWOpTypeTokenInfo.insert(std::make_pair(key, KWOpTypeInfo{value.type, value.value}));*/

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
            case eKWNamedOpTypeConst: {
                    const string_view kw(results.first, results.second);
                    auto it = g_KWOpTypeTokenInfo.find(kw);
                    if (it != g_KWOpTypeTokenInfo.end()) {
                        m_tokens.emplace_back(it->second.type, it->second.value, location());
                    }
                }
                break;
            case eSingleLineComment: {
                    const string_view comment(results.first+2, results.second);
                    auto ctok = pl::core::tkn::Literal::makeComment(true, string(comment));
                    m_tokens.emplace_back(ctok.type, ctok.value, location());
                }
                break;
            case eSingleLineDocComment: {
                    const string_view comment(results.first+3, results.second);
                    auto ctok = pl::core::tkn::Literal::makeDocComment(false, true, string(comment));
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
                            auto ctok = pl::core::tkn::Literal::makeComment(false, string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcomment_location);
                        }
                        break;
                    case MLLocalDocComment: {
                            auto ctok = pl::core::tkn::Literal::makeDocComment(false, false, string(comment));
                            m_tokens.emplace_back(ctok.type, ctok.value, mlcomment_location);
                        }
                        break;
                    case MLGlobalDocComment: {
                            auto ctok = pl::core::tkn::Literal::makeDocComment(true, false, string(comment));
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
                        auto ntok = pl::core::tkn::Literal::makeNumeric(num.value());
                        m_tokens.emplace_back(ntok.type, ntok.value, location());
                    }
                }
            }

            lexertl::lookup(g_sm, results);
        }

        return { m_tokens, collectErrors() };
    }

} // namespace pl::core
