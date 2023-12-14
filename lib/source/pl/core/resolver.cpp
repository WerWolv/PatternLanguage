#include <pl/core/resolver.hpp>

namespace pl::core {
    Resolver::result Resolver::resolve(const std::string&path) const {
        hlp::Result<api::Source, std::string> result;

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
                return result::err(fmt::format("No possible way to resolve path {} and no default resolver set", path));

            result = m_defaultResolver(path);
        }

        if (result.isOk()) {
            const auto [it, inserted] = m_sourceContainer.insert_or_assign(path, result.unwrap());
            return result::good(&it->second);
        }

        return result::err(result.unwrapErrs());
    }
}
