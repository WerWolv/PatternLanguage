/*
 * Build the lexer state machine.
 *
 * In debug builds we use the state machine directly.
 *
 * In Release builds it's used by the pre-build step to generate the "static"
 * lexer -- a precompiled implementation without any initialisation overhead.
 *
 * The lexertl17 lexing library is used. Its GitHub repo can be found here:
 * https://github.com/BenHanson/lexertl17
 *
 * This file and new_lexer.cpp are the only two files to include lexertl
 * (in addition to the lexer_gen project which builds the static lexer).
 */
#include <pl/core/new_lexer_sm.hpp>

#include <lexertl/runtime_error.hpp>
#include <lexertl/rules.hpp>
#include <lexertl/generator.hpp>
#include <lexertl/generate_cpp.hpp>
#include <lexertl/state_machine.hpp>

#include <string>
#include <sstream>
#include <cassert>

namespace pl::core {

    namespace {
        // Is c a special regex char?
        // Return true if is is so we can escape it.
        // Note that lexertl uses flex style regular expressions.
        inline bool mustEscape(char c)
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

        // Escape special regex chars.
        template <typename String>
            inline std::string escapeRegex(const String& s) {
                std::string result;
                result.reserve(s.size() * 2);

                for (char c : s) {
                    if (mustEscape(c))
                        result += '\\';
                    result += c;
                }

                return result;
            }

            inline std::string escapeRegex(const char *s) {
                return escapeRegex(std::string(s));
            }

    } // anonymous namespace

    void newLexerBuild(lexertl::state_machine &sm)
    {
        try {
            lexertl::rules rules;

            // lexertl uses flex style regular expressions. Some pitfalls to look out for:
            //  - " (quote) is a special character. If you want a literal quote, you need
            //    to escape it. Anything enclosed in unescaped quotes it interpreted literally
            //    and not as a regular expression.
            //
            //  - [^xyx]: this will match anything that's not x, y or z. INCLUDING newlines!!!

            rules.push_state("MLCOMMENT");      // we're lexing a multiline comment
            rules.push_state("DIRECTIVETYPE");
            rules.push_state("DIRECTIVEPARAM");

            rules.push("*", "[ \t\f\v]+", lexertl::rules::skip(), ".");

            rules.push("\r\n|\n|\r", LexerToken::NewLine);

            rules.push(R"(\/\/[^/][^\r\n]*)", LexerToken::SingleLineComment);
            rules.push(R"(\/\/\/[^\r\n]*)", LexerToken::SingleLineDocComment);

            rules.push("INITIAL", R"(\/\*[^*!\r\n]?)", LexerToken::MultiLineCommentOpen, "MLCOMMENT");
        
            rules.push("INITIAL", R"(\/\*[*!].*)", LexerToken::MultiLineDocCommentOpen, "MLCOMMENT");
            rules.push("MLCOMMENT", "\r\n|\n|\r", LexerToken::NewLine, ".");
            rules.push("MLCOMMENT", R"([^*\r\n]+|.)", lexertl::rules::skip(), "MLCOMMENT");
            rules.push("MLCOMMENT", R"(\*\/)", LexerToken::MultiLineCommentClose, "INITIAL");

            rules.push(R"([a-zA-Z_]\w*)", LexerToken::KWNamedOpTypeConstIdent);

            rules.push(
                "("
                "([0-9]+\\.[0-9]*|\\.[0-9]+)"   // group decimal alternatives here with '|'
                "([eE][+-]?[0-9]+)?"            // optional exponent
                "[fFdD]?"                       // optional suffix
                ")|"
                "("
                "[0-9]+[eE][+-]?[0-9]+"         // no decimal but exponent required
                "[fFdD]?"                       // optional suffix
                ")|"
                "("
                "[0-9]+"                        // no decimal, no exponent
                "[fFdD]"                        // suffix required
                ")",
                LexerToken::FPNumber
            );

            rules.push("(0[xXoObB])?[0-9a-fA-F]+('[0-9a-fA-F]+)*[uU]?", LexerToken::Integer);

            rules.push(R"(\"([^\"\r\n\\]|\\.)*\")", LexerToken::String);
            rules.push(R"('('|(\\'|[^'\r\n])+)')", LexerToken::Char);

            rules.push("INITIAL", R"(#[ \t\f\v]*(define|undef|ifdef|ifndef|endif))", LexerToken::Directive, ".");
            rules.push("INITIAL", R"(#[ \t\f\v]*[a-zA-Z_]\w*)", LexerToken::Directive, "DIRECTIVETYPE");
            rules.push("DIRECTIVETYPE", "\r\n|\n|\r", LexerToken::NewLine, "INITIAL");
            rules.push("DIRECTIVETYPE", R"(\S+)", LexerToken::DirectiveType, "DIRECTIVEPARAM");
            rules.push("DIRECTIVEPARAM", "\r\n|\n|\r", LexerToken::NewLine, "INITIAL");
            rules.push("DIRECTIVEPARAM", R"(\S.*)", LexerToken::DirectiveParam, "INITIAL");

            // The parser expects >= and <= as two separate tokens. Not sure why.
            // I originally intended to handle this differently but this (and other "split tokens")
            // make the longest-match rule useless. I will address this when I build a new parser.
            const char* ops[] = {"+", "-", "*", "/", "%", "&", "|", "^", "~", "==", "!=", "<", ">",
                                "&&", "||", "!", "^^", "$", ":", "::", "?", "@", "=", "addressof",
                                "sizeof", "typenameof"};    
            std::ostringstream opsSS; 
            for (auto op : ops) {
                opsSS << escapeRegex(op) << "|";
            }
            std::string oprs = opsSS.str();
            oprs.pop_back();
            rules.push(oprs, LexerToken::Operator);

            const std::string sepChars = escapeRegex("(){}[],.;");
            rules.push("["+sepChars+"]", LexerToken::Separator);

            lexertl::generator::build(rules, sm);
        }
        catch (const lexertl::runtime_error &e) {
            // Looks like a regex is invalid
            [[maybe_unused]] const char *what = e.what();
            assert(false);
            std::terminate();
        }
    }

} // namespace pl::core
