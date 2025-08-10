#pragma once

#include <lexertl/state_machine.hpp>

namespace pl::core {

    namespace {

        enum {
            eEOF, eNewLine, eKWNamedOpTypeConstIdent,
            eSingleLineComment, eSingleLineDocComment,
            eMultiLineCommentOpen, eMultiLineDocCommentOpen, eMultiLineCommentClose,
            eString, eSeparator, eDirective, eDirectiveType, eDirectiveParam,
            eOperator, eChar, eInteger, eFPNumber
        };
    } // anonymous namespace

    void newLexerBuild(lexertl::state_machine &sm);

} // namespace pl::core
