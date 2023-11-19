#pragma once

#include <pl/helpers/types.hpp>
#include <pl/api.hpp>
#include <utility>

namespace pl::core {

    using SourceResolver = std::function<hlp::Result<api::Source, std::string>(const std::string&)>;

    class Resolver {

    public:
        Resolver() = default;

        hlp::Result<api::Source*, std::string> resolve(const std::string& path);

        inline void registerProtocol(const std::string& protocol, const SourceResolver& resolver) {
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

        inline void setDefaultResolver(const SourceResolver& resolver) const {
            m_defaultResolver = resolver;
        }

    private:
        mutable std::map<std::string, SourceResolver> m_protocolResolvers; // for git://, http(s)://
        mutable SourceResolver m_defaultResolver;
        mutable std::map<std::string, api::Source> m_cachedSources;
    };

}