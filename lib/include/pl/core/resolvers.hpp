//
// Created by justus on 19.11.23.
//

#pragma once

#include <pl/core/resolver.hpp>
#include <wolv/io/fs.hpp>

namespace pl::core {
    class FileResolver final {
        using result = hlp::Result<api::Source, std::string>;
    public:

        FileResolver() = default;
        explicit FileResolver(const std::vector<std::fs::path>& includePaths) : m_includePaths(includePaths) { }

        hlp::Result<api::Source, std::string> resolve(const std::string& path) const;

        [[nodiscard]] const std::vector<std::fs::path>& getIncludePaths() const {
            return this->m_includePaths;
        }

        void setIncludePaths(const std::vector<std::fs::path>& includePaths) const {
            this->m_includePaths = includePaths;
        }

        api::Source* addVirtualFile(const std::string& code, const std::string& path) const {
            this->m_virtualFiles[path] = api::Source(code, path);
            return &this->m_virtualFiles[path];
        }

    private:
        mutable std::vector<std::fs::path> m_includePaths;
        mutable std::map<std::string, api::Source> m_virtualFiles;
    };
}