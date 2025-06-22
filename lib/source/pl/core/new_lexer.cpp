// lexertl_test.cpp
//

#include <pl/api.hpp>
#include <pl/core/token.hpp>
#include <pl/core/new_lexer.hpp>

//#include "out.cpp"

#include <lexertl/lookup.hpp>
#include <lexertl/state_machine.hpp>
#include <lexertl/generator.hpp>
#include <lexertl/generate_cpp.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <type_traits>

using std::string;
using std::cin;
using std::cout;
using std::endl;

const char* g_keywords[] = {
    "if",
    "else",
    "while",
    "for",
    "match",
    "return",
    "break",
    "continue",
    "struct",
    "enum",
    "union",
    "fn",
    "bitfield",
    "unsigned",
    "signed",
    "le",
    "be",
    "parent",
    "namespace",
    "using",
    "this",
    "in",
    "out",
    "ref",
    "null",
    "const",
    "_",
    "try",
    "catch",
    "import",
    "as",
    "is",
    "from"
};

const char* g_namedOperators[] =
{
    "addressof",
    "sizeof",
    "typenameof"
};

const char* g_operators[] =
{
    "+",
    "-",
    "*",
    "/",
    "%",
    // ls and rs are composed in the parser due to ambiguity with recursive templates
    "&",
    "|",
    "^",
    "~",
    "==",
    "!=",
    "<",
    ">",
    // ltoe and gtoe are also handled in the parser due to ambiguity with ls assignment
    "&&",
    "||",
    "!",
    "^^",
    "$",
    ":",
    "::",
    "?",
    "@",
    "=",
};

const char* g_types[] = {
   "padding",
   "auto",
   "any",
   "u8",
   "u16",
   "u24",
   "u32",
   "u48",
   "u64",
   "u96",
   "u128",
   "s8",
   "s16",
   "s24",
   "s32",
   "s48",
   "s64",
   "s96",
   "s128",
   "float",
   "double",
   "bool",
   "char",
   "char16",
   "str"
   /*
       // non named
       const auto Unsigned         = makeToken(Token::Type::ValueType, Token::ValueType::Unsigned);
       const auto Signed           = makeToken(Token::Type::ValueType, Token::ValueType::Signed);
       const auto FloatingPoint    = makeToken(Token::Type::ValueType, Token::ValueType::FloatingPoint);
       const auto Integer          = makeToken(Token::Type::ValueType, Token::ValueType::Integer);
       const auto CustomType       = makeToken(Token::Type::ValueType, Token::ValueType::CustomType);
   */
};

const char* g_seperators[] =
{
    "(",
    ")",
    "{",
    "}",
    "[",
    "]",
    ",",
    ".",
    ";"
    // EndOfProgram
};

const char* g_constants[] = {
    "true",
    "false",
    "nan",
    "inf"
};

const char* g_direcives[] = {
    "include",
    "define",
    "undef",
    "ifdef",
    "ifndef",
    "endif",
    "error",
    //"pragma"
};

string rx_directives()
{
    string result = string("#\\s*") + g_direcives[0];
    for (size_t i=1; i<sizeof(g_direcives)/sizeof(g_direcives[0]); ++i)
        result += string("|#\\s*") + g_direcives[i];
    return result;
}

template <typename T, std::size_t N>
string rx_strings(const T(&array)[N])
{
    string result(array[0]);
    for (size_t i=1; i<N; ++i)
        result += (string("|") + array[i]);
    return result;
}

inline bool must_escape(char c)
{
    switch (c) {
    case '+': case '/': case '*': case '?':
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

inline string escape_regex(const std::string& s) {
    string result;
    result.reserve(s.size() * 2);

    for (char c : s)
    {
        if (must_escape(c))
            result += '\\';
        result += c;
    }

    return result;
}

template <typename T, std::size_t N>
string rx_esc_strings(const T(&array)[N])
{
    std::string result(escape_regex(array[0]));
    for (size_t i = 1; i < sizeof(array) / sizeof(array[0]); ++i)
        result += ("|") + escape_regex(array[i]);
    return result;
}

enum {
    eEOF, eNewLine , eFirstAutoLexToken
};

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

string read_text_file(const string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/*int main()
{
    cout << "Dynamic (d), Generate (g) or Use gen (u): ";
    string answer;
    cin >> answer;

    string input = read_text_file(R"(C:\projects\SteveImHex\build-debug\install\patterns\pe.hexpat)");

    switch (answer[0])
    {
    case 'd':
    case 'D':
        dynamic(input);
        break;
    case 'g':
    case 'G':
        generate();
        break;
    case 'u':
    case 'U':
        usegen(input);
        break;
    }

    return 0;
}*/

namespace pl::core {

namespace {

struct ID2TokenInfo {
    Token::Type type;
    Token::ValueTypes value;
};

typedef std::vector<ID2TokenInfo> ID2Tok;
ID2Tok g_lexID2Token;
ID2Tok::size_type g_firstAutoID = 0;
ID2Tok::size_type g_OnePastLastAutoID = 0;

lexertl::state_machine g_sm;

inline bool isAutoID(ID2Tok::size_type id)
{
    return id>=g_firstAutoID && id<g_OnePastLastAutoID;
}

} // anon namespace

void init_new_lexer()
{
    lexertl::rules rules;

    rules.push("*", "\n", eNewLine, ".");
    rules.push("[ \t]", lexertl::rules::skip());

    int lexerid = eFirstAutoLexToken;
    g_firstAutoID = eFirstAutoLexToken;

    auto keywords = Token::Keywords();
    for (const auto& [key, value] : keywords) {
        rules.push(std::string(key).c_str(), lexerid);
        g_lexID2Token.emplace_back(value.type, value.value);
        ++lexerid;
    }

    /*auto opeators = Token::Operators();
    for (const auto& [key, value] : opeators) {
        g_lexID2Token.push_back(
            LexTokenInfo{
                value.type,
                (int)std::get<Token::Operator>(value.value)}
        );
        ++lexerid; 
    }

    auto types = Token::Types();
    for (const auto& [key, value] : types) {
        g_lexID2Token.push_back(
            LexTokenInfo{
                value.type,
                (int)std::get<Token::ValueType>(value.value)}
        );
        ++lexerid; 
    }

    auto separators = Token::Separators();
    for (const auto& [key, value] : separators) {
        g_lexID2Token.push_back(
            LexTokenInfo{
                value.type,
                (int)std::get<Token::Separator>(value.value)}
        );
        ++lexerid; 
    }

    auto directives = Token::Directives();
    for (const auto& [key, value] : directives) {
        g_lexID2Token.push_back(
            LexTokenInfo{
                value.type,
                (int)std::get<Token::Directive>(value.value)}
        );
        ++lexerid;
    }*/

    g_OnePastLastAutoID = lexerid;

    lexertl::generator::build(rules, g_sm);

    /*lexerid = 1;
    for (const auto &val : g_lexID2Token) {
        cout << lexerid++ << ": " << (int)val->type << " -> " << val->value << endl;
    }*/
}

hlp::CompileResult<std::vector<Token>> New_Lexer::lex(const api::Source *source)
{
    m_tokens.clear();

    lexertl::smatch results(source->content.begin(), source->content.end());
    auto line_start = results.first;
    u32 line = 1;

    auto location = [&]() -> Location {
        u32 column = results.first-line_start+1;
        size_t errorLength = results.second-results.first;
        return Location { source, line, column, errorLength };
    };

    lexertl::lookup(g_sm, results);
    while (results.id!=0)
    {
        if (results.id == eNewLine)
        {
            ++line;
            auto len = results.first - line_start; (void)len;
            line_start = results.second;
        }

        if (isAutoID(results.id))
        {
            const ID2TokenInfo &inf = g_lexID2Token.at(results.id-eFirstAutoLexToken);
            m_tokens.emplace_back(inf.type, inf.value, location()); 
                
        }

        /*if (results.id == eBreakPoint)
        {
            int a = 0; (void)a;
        }*/

        lexertl::lookup(g_sm, results);
    }

    return {};
}

} // namespace pl::core
