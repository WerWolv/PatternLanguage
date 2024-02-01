#pragma once

#include <pl/api.hpp>
#include <pl/core/ast/ast_node.hpp>

#include <pl/core/errors/result.hpp>

#include <set>

namespace pl::core {

    class ParserManager {
    public:
        explicit ParserManager() = default;

        hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> parse(api::Source* source, const std::string &namespacePrefix = "");

        void setResolver(const api::Resolver& resolver) {
            m_resolver = resolver;
        }

        void setPatternLanguage(PatternLanguage* patternLanguage) {
            m_patternLanguage = patternLanguage;
        }

        [[nodiscard]] const api::Resolver& getResolver() const {
            return m_resolver;
        }

        void reset() {
            this->m_onceIncluded.clear();
        }

    private:
        api::Resolver m_resolver = nullptr;
        PatternLanguage* m_patternLanguage = nullptr;
        std::set<api::Source*> m_onceIncluded;
    };

}