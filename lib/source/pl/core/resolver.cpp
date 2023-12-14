#include <pl/core/resolver.hpp>

using namespace pl::core;
using namespace pl::api;
using namespace pl::hlp;

Result<Source *, std::string> pl::core::Resolver::resolve(const std::string &path) const {
    using result_t = Result<Source *, std::string>;
    Result<Source, std::string> result;

    // look for protocol
    if(!m_protocolResolvers.empty()){
        if (const auto protocolEnd = path.find("://"); protocolEnd != std::string::npos) {
            const auto protocol = path.substr(0, protocolEnd);
            if (const auto protocolIt = m_protocolResolvers.find(protocol); protocolIt != m_protocolResolvers.end()) {
                result = protocolIt->second(path);
            }
        }
    }

    if(!result.isOk()) {
        if (m_defaultResolver == nullptr)
            return result_t::err("No possible way to resolve path " + path + " and no default resolver set");

        result = m_defaultResolver(path);
    }

    if (result.isOk()) {
        const auto [it, inserted] = m_sourceContainer.insert_or_assign(path, result.unwrap());
        return result_t::good(&it->second);
    }

    return result_t::err(result.unwrapErrs());
}
