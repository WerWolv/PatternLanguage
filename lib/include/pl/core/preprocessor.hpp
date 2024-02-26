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

        hlp::CompileResult<std::vector<pl::core::Token>> preprocess(PatternLanguage *runtime, api::Source* source, bool initialRun = true);

        void addDefine(const std::string &name, const std::string &value = "");
        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void addDirectiveHandler(const std::string &directiveType, const api::DirectiveHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);
        void removeDirectiveHandler(const std::string &directiveType);

        std::optional<pl::core::Token> getDirectiveValue(unsigned line);

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

        inline std::string peek(i32 p = 0) const {
            if( (m_token + p)  >=  m_result.unwrap().end()) return "EOF";
            return (m_token+p)->getFormattedValue();
        }

        std::string parseDirectiveName();

        // directive handlers
        void handleIfDef(unsigned line);
        void handleIfNDef(unsigned line);
        void handleDefine(unsigned line);
        void handleUnDefine(unsigned line);
        void handlePragma(unsigned line);
        void handleInclude(unsigned line);
        void handleError(unsigned line);

        void process();
        void processIfDef(bool add);

        void registerDirectiveHandler(const std::string& name, auto memberFunction);

        std::unordered_map<std::string, api::PragmaHandler> m_pragmaHandlers;
        std::unordered_map<std::string, api::DirectiveHandler> m_directiveHandlers;

        std::unordered_map<std::string, std::vector<pl::core::Token>> m_defines;
        std::unordered_map<std::string, std::vector<std::pair<std::string, u32>>> m_pragmas;

        std::set<std::string> m_onceIncludedFiles;

        api::Resolver m_resolver = nullptr;
        PatternLanguage *m_runtime = nullptr;

        std::vector<pl::core::Token> m_keys;
        std::atomic<bool> m_initialized = false;
        __gnu_cxx::__normal_iterator<pl::core::Token *, std::vector<pl::core::Token>> m_token;
        hlp::CompileResult<std::vector<pl::core::Token>> m_result;
        std::vector<pl::core::Token> m_output;

        api::Source* m_source = nullptr;

        bool m_onlyIncludeOnce = false;
    };

}