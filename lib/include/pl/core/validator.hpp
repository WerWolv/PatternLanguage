#pragma once

#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/errors/result.hpp>

namespace pl::core {

    namespace ast { class ASTNode; }

    class Validator : err::ErrorCollector {
    public:
        Validator() = default;

        hlp::CompileResult<bool> validate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast);

        void setRecursionDepth(const u32 limit) {
            this->m_maxRecursionDepth = limit;
        }

    private:
        bool validateNodes(const std::vector<std::unique_ptr<ast::ASTNode>> &nodes, bool newScope = true);
        bool validateNodes(const std::vector<std::shared_ptr<ast::ASTNode>> &nodes, bool newScope = true);
        bool validateNode(const std::shared_ptr<ast::ASTNode>& node, bool newScope = true);
        bool validateNode(ast::ASTNode *node, std::unordered_set<std::string>& identifiers);

        Location location() override;

        u32 m_maxRecursionDepth = 32;
        u32 m_recursionDepth = 0;

        ast::ASTNode *m_lastNode = nullptr;
        std::set<ast::ASTNode*> m_validatedNodes;
        std::list<std::unordered_set<std::string>> m_identifiers;
    };

}
