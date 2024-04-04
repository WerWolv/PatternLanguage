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
        struct ExcludedLocation {
            bool isExcluded;
            Location location;
        };
        Preprocessor();

        ~Preprocessor() override = default;

        hlp::CompileResult<std::vector<Token>> preprocess(PatternLanguage *runtime, api::Source* source, bool initialRun = true);

        void addDefine(const std::string &name, const std::string &value = "");
        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void addDirectiveHandler(const Token::Directive &directiveType, const api::DirectiveHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);
        void removeDirectiveHandler(const Token::Directive &directiveType);
        void validateExcludedLocations();
        void appendExcludedLocation(const ExcludedLocation &location);
        void validateOutput();

        [[nodiscard]] auto getExcludedLocations() const {
            return m_excludedLocations;
        }

        [[nodiscard]] auto getResult() const {
            return this->m_result;
        }

        [[nodiscard]] auto getOutput() const {
            return this->m_output;
        }

        void setOutput(std::vector<pl::core::Token> tokens) {
            m_output = tokens;
        }

        [[nodiscard]] auto getErrors() const {
            return this->m_errors;
        }

        void setErrors(std::vector<err::CompileError> errors) {
            m_errors = errors;
        }

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

        bool isInitialized() const {
            return m_initialized;
        }

        void setResolver(const api::Resolver& resolvers) {
            m_resolver = resolvers;
        }

        auto getNamespaces() const {
            return m_namespaces;
        }

        void appendToNamespaces(std::vector<Token> namespaces);

    private:
        Preprocessor(const Preprocessor &);
        bool eof();
        Location location() override;
        void removeKey(const Token &token);
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
        std::vector<ExcludedLocation> m_excludedLocations;

        std::set<std::string> m_onceIncludedFiles;

        api::Resolver m_resolver = nullptr;
        PatternLanguage *m_runtime = nullptr;

        std::vector<Token> m_keys;
        std::atomic<bool> m_initialized = false;
        std::vector<Token>::iterator m_token;
        std::vector<err::CompileError> m_errors;
        std::vector<Token> m_result;
        std::vector<Token> m_output;
        std::vector<std::string> m_namespaces;

        api::Source* m_source = nullptr;

        bool m_onlyIncludeOnce = false;
    };

}