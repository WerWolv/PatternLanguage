//
// Created by justus on 19.11.23.
//

#pragma once

#include <pl/core/resolver.hpp>

namespace pl::core {
    class FileResolver {
    public:

        FileResolver() = default;
        explicit FileResolver(const std::vector<std::fs::path>& includePaths) : m_includePaths(includePaths) { }

        hlp::Result<api::Source, std::string> resolve(const std::string& path);
        inline hlp::Result<api::Source, std::string> operator()(const std::string& path) {
            return resolve(path);
        }

        void setIncludePaths(const std::vector<std::fs::path>& includePaths) const {
            this->m_includePaths = includePaths;
        }

    private:
        mutable std::vector<std::fs::path> m_includePaths;
    };
}