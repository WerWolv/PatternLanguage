#pragma once

#include <lexertl/state_machine.hpp>

namespace pl::core {

    namespace {

        namespace LexerToken {

            enum {
                EndOfFile, NewLine, KWNamedOpTypeConstIdent,
                SingleLineComment, SingleLineDocComment,
                MultiLineCommentOpen, MultiLineDocCommentOpen, MultiLineCommentClose,
                String, Separator, Directive, DirectiveType, DirectiveParam,
                Operator, Char, Integer, FPNumber
            };

        }
    }

    void newLexerBuild(lexertl::state_machine &sm);

}
