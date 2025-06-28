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
//#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <type_traits>

using std::string;
using std::string_view;
using std::cin;
using std::cout;
using std::endl;

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

namespace {

struct KWTokenInfo {
    Token::Type type;
    Token::ValueTypes value;
};

struct TransHash {
    using is_transparent = void;

    std::size_t operator()(const std::string &s) const noexcept {
        return std::hash<string>{}(s);
    }

    std::size_t operator()(std::string_view s) const noexcept {
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

    bool operator()(std::string_view lhs, const std::string& rhs) const noexcept {
        return lhs == rhs;
    }

    bool operator()(string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
};

std::unordered_map<std::string, KWTokenInfo, TransHash, TransEqual> g_KW2TokenInfo;

lexertl::state_machine g_sm;

enum {
    eEOF, eNewLine, eKWNamedOpTypeConst
};

} // anon namespace

void init_new_lexer()
{
    lexertl::rules rules;

    rules.push("\n", eNewLine);
    rules.push(R"([a-zA-Z_]\w*)", eKWNamedOpTypeConst);

    auto keywords = Token::Keywords();
    for (const auto& [key, value] : keywords)
        g_KW2TokenInfo.insert(std::make_pair(key, KWTokenInfo{value.type, value.value}));

    auto opeators = Token::Operators();
    for (const auto& [key, value] : opeators)
        g_KW2TokenInfo.insert(std::make_pair(key, KWTokenInfo{value.type, value.value}));

    lexertl::generator::build(rules, g_sm);
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
    (void)location;

    lexertl::lookup(g_sm, results);
    while (results.id!=0)
    {
        //cout << "#" << results.id << " : " << string_view(results.first, results.second) << endl;
        
        if (results.id == eNewLine)
        {
            //cout << "#" << line << " : " << string_view(line_start, results.first) << endl;

            ++line;
            auto len = results.first - line_start; (void)len;
            line_start = results.second;
        }

        //if (isAutoID(results.id))
        if (results.id == eKWNamedOpTypeConst)
        {
            const string_view kw(results.first, results.second);
            auto it = g_KW2TokenInfo.find(kw);
            if (it != g_KW2TokenInfo.end()) {
                m_tokens.emplace_back(it->second.type, it->second.value, location()); 
            }
        }

        lexertl::lookup(g_sm, results);
    }

    return {};
}

} // namespace pl::core
