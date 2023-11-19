#include <pl/core/resolver.hpp>

using namespace pl::core;
using namespace pl::api;
using namespace pl::hlp;

Result<Source *, std::string> Resolver::resolve(const std::string &path)  {
    using result_t = Result<Source *, std::string>;
    // look in cache
    auto it = m_cachedSources.find(path);
    if (it != m_cachedSources.end())
        return result_t::good(&it->second);

    Result<Source, std::string> result;

    // look for protocol
    if(!m_protocolResolvers.empty()){
        auto protocolEnd = path.find("://");
        if (protocolEnd != std::string::npos) {
            auto protocol = path.substr(0, protocolEnd);
            auto protocolIt = m_protocolResolvers.find(protocol);
            if (protocolIt != m_protocolResolvers.end()) {
                result = protocolIt->second(path);
            }
        }
    }

    if(!result.is_ok()) {
        if (!m_defaultResolver)
            return result_t::err("No possible way to resolve path " + path + " and no default resolver set");

        result = m_defaultResolver(path);
    }

    if (result.is_ok()) {
        m_cachedSources[path] = result.unwrap();
        return result_t::good(&m_cachedSources[path]);
    }

    return result_t::err(result.unwrap_errs());
}