#pragma once

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <pl/helpers/types.hpp>
#include <pl/core/errors/validator_errors.hpp>

namespace pl::core {

    namespace ast { class ASTNode; }

    class Validator {
    public:
        Validator() = default;

        bool validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast, bool newScope = true, bool firstRun = false);
        bool validate(const std::string &sourceCode, const std::vector<std::unique_ptr<ast::ASTNode>> &ast, bool newScope = true, bool firstRun = false);

        const std::optional<err::PatternLanguageError> &getError() { return this->m_error; }

        void setRecursionDepth(u32 limit) {
            this->m_maxRecursionDepth = limit;
        }

    private:
        std::optional<err::PatternLanguageError> m_error;

        u32 m_maxRecursionDepth = 32;
        u32 m_recursionDepth = 0;

        ast::ASTNode *m_lastNode = nullptr;
        std::set<ast::ASTNode*> m_validatedNodes;
        std::list<std::unordered_set<std::string>> m_identifiers;
    };

}