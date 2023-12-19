#pragma once

#include <pl/helpers/types.hpp>
#include <pl/api.hpp>
#include <utility>

namespace pl::core {

    using SourceResolver = std::function<hlp::Result<api::Source, std::string>(const std::string&)>;

    class Resolver {

        using Result = hlp::Result<api::Source*, std::string>;

    public:
        Resolver() = default;

        Result resolve(const std::string &path) const;

        void registerProtocol(const std::string &protocol, const SourceResolver &resolver) const {
            m_protocolResolvers[protocol] = resolver;
        }

        void setDefaultResolver(const SourceResolver &resolver) const {
            m_defaultResolver = resolver;
        }

    private:
        mutable std::map<std::string, SourceResolver> m_protocolResolvers; // for git://, http(s)://
        mutable SourceResolver m_defaultResolver;
        mutable std::map<std::string, api::Source> m_sourceContainer;
    };

}