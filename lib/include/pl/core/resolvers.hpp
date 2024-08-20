#pragma once

#include <pl/core/resolver.hpp>
#include <wolv/io/fs.hpp>

namespace pl::core::resolvers {

    using Result = hlp::Result<api::Source, std::string>;
    
    class FileResolver final {
    public:

        FileResolver() = default;
        explicit FileResolver(const std::vector<std::fs::path>& includePaths) : m_includePaths(includePaths) { }

        Result resolve(const std::string &path) const;

        [[nodiscard]] const std::vector<std::fs::path>& getIncludePaths() const {
            return this->m_includePaths;
        }

        void setIncludePaths(const std::vector<std::fs::path> &includePaths) const {
            this->m_includePaths = includePaths;
        }

        api::Source* addVirtualFile(const std::string &code, const std::string &path, bool mainSource = false) const {
            this->m_virtualFiles[path] = api::Source(code, path, mainSource);
            return &this->m_virtualFiles[path];
        }

    private:
        mutable std::vector<std::fs::path> m_includePaths;
        mutable std::map<std::string, api::Source> m_virtualFiles;
    };
}