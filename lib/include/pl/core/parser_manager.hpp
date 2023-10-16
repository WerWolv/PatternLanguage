#pragma once

#include <pl/api.hpp>
#include <pl/core/ast/ast_node.hpp>
#include <pl/core/errors/error.hpp>

namespace pl::core {

    using AST = std::vector<std::shared_ptr<ast::ASTNode>>;

    class ParserManager {

        explicit ParserManager(const api::IncludeResolver& resolver) : m_includeResolver(resolver) {}

        CompileResult<AST> parse(const std::string &code);

    private:
        std::map<std::string, AST> m_astCache;
        api::IncludeResolver m_includeResolver;
    };

}