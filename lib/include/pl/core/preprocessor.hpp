#pragma once


#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include <pl/api.hpp>
#include <pl/helpers/types.hpp>
#include <pl/core/errors/preprocessor_errors.hpp>

#include <wolv/io/fs.hpp>

namespace pl::core {

    class Preprocessor {
    public:
        Preprocessor();

        std::optional<std::string> preprocess(PatternLanguage &runtime, const std::string &code, bool initialRun = true);

        void addDefine(const std::string &name, const std::string &value = "");
        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);

        void setIncludePaths(std::vector<std::fs::path> paths) {
            this->m_includePaths = std::move(paths);
        }

        const std::optional<err::PatternLanguageError> &getError() { return this->m_error; }

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

    private:
        Preprocessor(const Preprocessor &);

    private:
        std::unordered_map<std::string, api::PragmaHandler> m_pragmaHandlers;

        std::unordered_map<std::string, std::pair<std::string, u32>> m_defines;
        std::unordered_map<std::string, std::vector<std::pair<std::string, u32>>> m_pragmas;

        std::set<std::fs::path> m_onceIncludedFiles;

        std::optional<err::PatternLanguageError> m_error;

        bool m_onlyIncludeOnce = false;

        std::vector<std::fs::path> m_includePaths;
    };

}