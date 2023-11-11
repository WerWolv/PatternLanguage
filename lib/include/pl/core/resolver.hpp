#pragma once

#include <pl/helpers/types.hpp>
#include <pl/api.hpp>
#include <utility>

namespace pl::core {

    using Resolver = std::function<hlp::Result<api::Source, std::string>(const std::string&)>;

    class Resolvers {

    public:
        Resolvers() = default;

        hlp::Result<api::Source*, std::string> resolve(const std::string& path);
        inline hlp::Result<api::Source*, std::string> operator()(const std::string& path) { // forward for include resolver functional interface
            return resolve(path);
        }

        inline void registerProtocol(const std::string& protocol, const Resolver& resolver) {
            m_protocolResolvers[protocol] = resolver;
        }

        inline api::Source* addSource(const std::string& path, const api::Source& source) const {
            return &m_cachedSources.emplace(path, source).first->second;
        }

        inline api::Source* setSource(const std::string& path, const api::Source& source) const {
            m_cachedSources[path] = source;
            return &m_cachedSources[path];
        }

        inline api::Source* addSource(const std::string& code, const std::string& source) const {
            return addSource(source, api::Source(code, source));
        }

        inline api::Source* setSource(const std::string& code, const std::string& source) const {
            return setSource(source, api::Source(code, source));
        }

        inline void setDefaultResolver(const Resolver& resolver) const {
            m_defaultResolver = resolver;
        }

    private:
        mutable std::map<std::string, Resolver> m_protocolResolvers; // for git://, http(s)://
        mutable Resolver m_defaultResolver;
        mutable std::map<std::string, api::Source> m_cachedSources;
    };

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