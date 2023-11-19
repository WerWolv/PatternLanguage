#pragma once

#include <pl/api.hpp>
#include <pl/core/ast/ast_node.hpp>
#include <pl/core/errors/error.hpp>
#include <pl/core/resolver.hpp>

#include <utility>

namespace pl::core {

    class ParserManager {
    public:
        explicit ParserManager() = default;

        CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> parse(const api::Source* source, const std::string &namespacePrefix = "");

        void setResolvers(Resolver* resolvers) {
            m_resolvers = resolvers;
        }

    private:
        std::map<std::string, std::vector<std::shared_ptr<ast::ASTNode>>> m_astCache;
        Resolver* m_resolvers = nullptr;
    };

}