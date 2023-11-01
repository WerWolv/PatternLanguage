#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

#include <pl/api.hpp>
#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <utility>
#include <wolv/io/fs.hpp>

namespace pl::core {

    class Preprocessor : err::ErrorCollector {
    public:

        Preprocessor();

        ~Preprocessor() override = default;

        CompileResult<std::string> preprocess(PatternLanguage *runtime, api::Source* source, bool initialRun = true);

        void addDefine(const std::string &name, const std::string &value = "");
        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void addDirectiveHandler(const std::string &directiveType, const api::DirectiveHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);
        void removeDirectiveHandler(const std::string &directiveType);

        std::optional<std::string> getDirectiveValue(bool allowWhitespace = false);

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

        void setIncludeResolver(const api::IncludeResolver &resolver) {
            this->m_includeResolver = resolver;
        }

    private:
        Preprocessor(const Preprocessor &);

    private:
        Location location() override;

        inline char peek(i32 p = 0) const {
            return m_offset + p < m_code.size() ? m_code[m_offset + p] : '\0';
        }

        std::string parseDirectiveName();
        void parseComment();

        // directive handlers
        void handleIfDef();
        void handleIfNDef();
        void handleDefine();
        void handlePragma();
        void handleInclude();
        void handleError();

        bool process();
        void processIfDef(bool add);

        void registerDirectiveHandler(const std::string& name, auto memberFunction);

        std::unordered_map<std::string, api::PragmaHandler> m_pragmaHandlers;
        std::unordered_map<std::string, api::DirectiveHandler> m_directiveHandlers;

        std::unordered_map<std::string, std::pair<std::string, u32>> m_defines;
        std::unordered_map<std::string, std::vector<std::pair<std::string, u32>>> m_pragmas;

        std::set<std::fs::path> m_onceIncludedFiles;

        api::IncludeResolver m_includeResolver;

        PatternLanguage *m_runtime = nullptr;

        u32 m_offset = 0;
        u32 m_lineNumber = 1;
        u32 m_lineBeginOffset = 0;
        bool m_inString = false;
        bool m_startOfLine = true;
        std::string m_output;

        std::string m_code;
        api::Source* m_source = nullptr;

        bool m_onlyIncludeOnce = false;
    };

}