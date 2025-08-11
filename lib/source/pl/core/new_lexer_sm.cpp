#include <pl/core/new_lexer_sm.hpp>

#include <lexertl/rules.hpp>
#include <lexertl/generator.hpp>
#include <lexertl/generate_cpp.hpp>
#include <lexertl/state_machine.hpp>

#include <string>
#include <sstream> // TODO: replace with fmt?
#include <iostream> // DEBUGGING

namespace pl::core {

    namespace {
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
        lexertl::rules rules;

        rules.push_state("MLCOMMENT");
        rules.push_state("DIRECTIVETYPE");
        rules.push_state("DIRECTIVEPARAM");

        // Note:
        // This isn't in the "*" state because although a "." wont match newlines
        // (as I've configured lexertl), rules like "[^abc]" will. Safest to just
        // add it in other states explictly.
        rules.push("\r\n|\n|\r", eNewLine);

        rules.push(R"(\/\/[^/][^\r\n]*)", eSingleLineComment);
        rules.push(R"(\/\/\/[^\r\n]*)", eSingleLineDocComment);

        //rules.push("INITIAL", R"(\/\*[^*!\r\n].*)", eMultiLineCommentOpen, "MLCOMMENT");
        rules.push("INITIAL", R"(\/\*[^*!\r\n]?)", eMultiLineCommentOpen, "MLCOMMENT");
    
        rules.push("INITIAL", R"(\/\*[*!].*)", eMultiLineDocCommentOpen, "MLCOMMENT");
        rules.push("MLCOMMENT", "\r\n|\n|\r", eNewLine, ".");
        rules.push("MLCOMMENT", R"([^*\r\n]+|.)", lexertl::rules::skip(), "MLCOMMENT");
        rules.push("MLCOMMENT", R"(\*\/)", eMultiLineCommentClose, "INITIAL");

        rules.push(R"([a-zA-Z_]\w*)", eKWNamedOpTypeConstIdent);

        rules.push("[0-9]+[.][0-9]*([eE][+-]?[0-9]+)?[fFdD]?", eFPNumber);
        rules.push("(0[xXoObB])?[0-9a-fA-F]+('[0-9a-fA-F]+)*[uU]?", eInteger);
        //rules.push(R"([0-9][0-9a-fA-F'xXoOpP.uU+-]*)", eNumber);

        rules.push(R"(\"([^\"\r\n\\]|\\.)*\")", eString);
        rules.push(R"('([^\'\r\n\\]|\\.)')", eChar);

        rules.push("INITIAL", R"(#\s*(define|undef|ifdef|ifndef|endif))", eDirective, ".");
        rules.push("INITIAL", R"(#\s*[a-zA-Z_]\w*)", eDirective, "DIRECTIVETYPE");
        rules.push("DIRECTIVETYPE", R"([_a-zA-Z][_a-zA-Z0-9]*)", eDirectiveType, "DIRECTIVEPARAM");
        rules.push("DIRECTIVEPARAM", "\r\n|\n|\r", eNewLine, "INITIAL");
        rules.push("DIRECTIVEPARAM", R"(\S.*)", eDirectiveParam, "INITIAL");

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
        rules.push(oprs, eOperator);

        std::string sepChars = escapeRegex("(){}[],.;");
        rules.push("["+sepChars+"]", eSeparator);

        lexertl::generator::build(rules, sm);
    }

} // namespace pl::core
