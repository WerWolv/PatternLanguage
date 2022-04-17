#pragma once

#include <helpers/types.hpp>

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include <helpers/fs.hpp>

#include <pl/error.hpp>
#include <pl/api.hpp>

namespace pl {

    class Preprocessor {
    public:
        Preprocessor();

        std::optional<std::string> preprocess(PatternLanguage &runtime, std::string code, bool initialRun = true);

        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);

        void setIncludePaths(std::vector<std::fs::path> paths) {
            this->m_includePaths = std::move(paths);
        }

        const std::optional<PatternLanguageError> &getError() { return this->m_error; }

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

    private:
        [[noreturn]] static void throwPreprocessorError(const std::string &error, u32 lineNumber) {
            throw PatternLanguageError(lineNumber, "Preprocessor: " + error);
        }

        std::unordered_map<std::string, api::PragmaHandler> m_pragmaHandlers;

        std::set<std::tuple<std::string, std::string, u32>> m_defines;
        std::set<std::tuple<std::string, std::string, u32>> m_pragmas;

        std::set<std::filesystem::path> m_onceIncludedFiles;

        std::optional<PatternLanguageError> m_error;

        bool m_onlyIncludeOnce = false;

        std::vector<std::fs::path> m_includePaths;
    };

}