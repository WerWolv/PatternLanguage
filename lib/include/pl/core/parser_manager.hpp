#pragma once

#include <pl/api.hpp>
#include <pl/core/ast/ast_node.hpp>

#include <pl/core/errors/result.hpp>

namespace pl::core {

    class ParserManager {
    public:
        explicit ParserManager() = default;

        hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> parse(const api::Source* source, const std::string &namespacePrefix = "");

        void setResolver(const api::Resolver& resolver) {
            m_resolver = resolver;
        }

    private:
        api::Resolver m_resolver = nullptr;
    };

}