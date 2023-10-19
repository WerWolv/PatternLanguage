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

        CompileResult<std::string> preprocess(PatternLanguage &runtime, const std::string &code, bool initialRun = true);

        void addDefine(const std::string &name, const std::string &value = "");
        void addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler);
        void removePragmaHandler(const std::string &pragmaType);

        [[nodiscard]] bool shouldOnlyIncludeOnce() const {
            return this->m_onlyIncludeOnce;
        }

    private:
        Preprocessor(const Preprocessor &);

    private:
        Location location() override;

        std::unordered_map<std::string, api::PragmaHandler> m_pragmaHandlers;

        std::unordered_map<std::string, std::pair<std::string, u32>> m_defines;
        std::unordered_map<std::string, std::vector<std::pair<std::string, u32>>> m_pragmas;

        std::set<std::fs::path> m_onceIncludedFiles;

        api::IncludeResolver m_includeResolver;

        u32 m_offset = 0;
        u32 m_lineNumber = 1;

        bool m_onlyIncludeOnce = false;
    };

}