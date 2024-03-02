#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <atomic>

#include <pl/api.hpp>
#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/errors/result.hpp>

#include <utility>

namespace pl::core {

    class Preprocessor : public err::ErrorCollector {
    public:

        Preprocessor();

        ~Preprocessor() override = default;

        hlp::CompileResult<std::vector<Token>> preprocess(PatternLanguage *runtime, api::Source* source, bool initialRun = true);

        void addDefine(const std::string &name, const std::string &value = "");
        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void addDirectiveHandler(const Token::Directive &directiveType, const api::DirectiveHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);
        void removeDirectiveHandler(const Token::Directive &directiveType);

        std::optional<Token> getDirectiveValue(u32 line);

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

        bool isInitialized() const {
            return m_initialized;
        }

        void setResolver(const api::Resolver& resolvers) {
            m_resolver = resolvers;
        }

    private:
        Preprocessor(const Preprocessor &);

        Location location() override;

        // directive handlers
        void handleIfDef(u32 line);
        void handleIfNDef(u32 line);
        void handleDefine(u32 line);
        void handleUnDefine(u32 line);
        void handlePragma(u32 line);
        void handleInclude(u32 line);
        void handleError(u32 line);

        void process();
        void processIfDef(bool add);

        void registerDirectiveHandler(const Token::Directive &name, auto memberFunction);

        std::unordered_map<std::string, api::PragmaHandler> m_pragmaHandlers;
        std::unordered_map<Token::Directive, api::DirectiveHandler> m_directiveHandlers;

        std::unordered_map<std::string, std::vector<Token>> m_defines;
        std::unordered_map<std::string, std::vector<std::pair<std::string, u32>>> m_pragmas;

        std::set<std::string> m_onceIncludedFiles;

        api::Resolver m_resolver = nullptr;
        PatternLanguage *m_runtime = nullptr;

        std::vector<Token> m_keys;
        std::atomic<bool> m_initialized = false;
        std::vector<Token>::iterator m_token;
        hlp::CompileResult<std::vector<Token>> m_result;
        std::vector<Token> m_output;

        api::Source* m_source = nullptr;

        bool m_onlyIncludeOnce = false;
    };

}