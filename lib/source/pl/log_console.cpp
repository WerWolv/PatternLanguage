#include "pl/log_console.hpp"

#include "pl/ast/ast_node.hpp"

namespace pl {

    [[noreturn]] void LogConsole::abortEvaluation(const std::string &message, const ASTNode *node) {
        if (node == nullptr)
            throw PatternLanguageError(0, "Evaluator: " + message);
        else
            throw PatternLanguageError(node->getLine(), "Evaluator: " + message);
    }

}