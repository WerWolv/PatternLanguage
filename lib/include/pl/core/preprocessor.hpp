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

        void validateOutput();

        [[nodiscard]] const std::vector<ExcludedLocation>& getExcludedLocations() const {
            return m_excludedLocations;
        }

        [[nodiscard]] const std::vector<Token>& getResult() {
            return m_result;
        }

        [[nodiscard]] auto getOutput() const {
            return this->m_output;
        }

        void setOutput(const std::vector<pl::core::Token> &tokens) {
            u32 j =0;
            auto tokenCount = m_result.size();
            for (auto token : tokens) {
                if (auto identifier = std::get_if<Token::Identifier>(&token.value); identifier != nullptr) {
                    if (auto type = identifier->getType(); type > Token::Identifier::IdentifierType::ScopeResolutionUnknown) {
                        auto location = token.location;
                        if (location.source->source != "<Source Code>")
                            continue;
                        auto line = location.line;
                        auto column = location.column;
                        while (m_result[j].location.line < line) {
                            if (j >= tokenCount)
                                break;
                            j++;
                        }
                        while (m_result[j].location.column < column) {
                            if (j >= tokenCount)
                                break;
                            j++;
                        }
                        if (auto identifier2 = std::get_if<Token::Identifier>(&m_result[j].value); identifier2 != nullptr)
                            identifier2->setType(type);
                    }
                }
            }
        }

        [[nodiscard]] const std::vector<err::CompileError>& getStoredErrors() const {
            return this->m_storedErrors;
        }

        void setStoredErrors(const std::vector<err::CompileError> &errors) {
            m_storedErrors = errors;
        }

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

        bool isInitialized() const {
            return m_initialized;
        }

        [[nodiscard]] const api::Resolver& getResolver() const {
            return m_resolver;
        }

        void setResolver(const api::Resolver &resolvers) {
            m_resolver = resolvers;
        }

        const std::vector<std::string> &getNamespaces() const {
            return m_namespaces;
        }

        void appendToNamespaces(std::vector<Token> tokens);

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
        std::vector<err::CompileError> m_storedErrors;
        std::vector<Token> m_result;
        std::vector<Token> m_output;
        std::vector<std::string> m_namespaces;

        api::Source* m_source = nullptr;

        bool m_onlyIncludeOnce = false;
    };

}