#pragma once

#include <pl/api.hpp>
#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/helpers/safe_pointer.hpp>

#include <pl/core/errors/result.hpp>

#include <set>

namespace pl::core {

    class ParserManager {
    public:
        explicit ParserManager() = default;

        struct ParsedData {
            std::vector<std::shared_ptr<ast::ASTNode>> astNodes;
            std::map<std::string, hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>> types;
        };

        hlp::CompileResult<ParsedData> parse(api::Source* source, const std::string &namespacePrefix = "");

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
        struct OnceIncludePair {
            api::Source* source;
            std::string alias;

            auto operator<=>(const OnceIncludePair& other) const {
                return std::tie(*this->source, this->alias) <=> std::tie(*other.source, other.alias);
            }
        };

        std::map<OnceIncludePair, std::map<std::string, hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>>> m_parsedTypes {};
        std::set<OnceIncludePair> m_onceIncluded {};
        api::Resolver m_resolver = nullptr;
        PatternLanguage* m_patternLanguage = nullptr;
    };

}