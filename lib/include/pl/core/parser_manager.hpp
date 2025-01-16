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
            this->m_parsedTypes.clear();
        }

        struct OnceIncludePair {
            api::Source* source;
            std::string alias;

            auto operator<=>(const OnceIncludePair& other) const {
                return std::tie(*this->source, this->alias) <=> std::tie(*other.source, other.alias);
            }
        };
        std::set<OnceIncludePair> & getOnceIncluded() { return m_onceIncluded; }
        std::set<OnceIncludePair> & getPreprocessorOnceIncluded() { return m_preprocessorOnceIncluded; }
        void setPreprocessorOnceIncluded(const std::set<OnceIncludePair>& onceIncluded) { m_preprocessorOnceIncluded = onceIncluded; }

        ast::ASTNodeTypeDecl* addBuiltinType(const std::string &name, api::FunctionParameterCount parameterCount, const api::TypeCallback &func);
        const std::map<std::string, hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>>& getBuiltinTypes() const {
            return m_builtinTypes;
        }

private:
        std::map<OnceIncludePair, std::map<std::string, hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>>> m_parsedTypes;
        std::map<std::string, hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>> m_builtinTypes;
        std::set<OnceIncludePair> m_onceIncluded {};
        std::set<OnceIncludePair> m_preprocessorOnceIncluded {};
        api::Resolver m_resolver = nullptr;
        PatternLanguage* m_patternLanguage = nullptr;
    };

}