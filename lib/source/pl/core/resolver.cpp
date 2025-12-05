#include <pl/core/resolver.hpp>

#include <fmt/core.h>

namespace pl::core {
    Resolver::Result Resolver::resolve(const std::string &path) const {
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
                return Result::err(fmt::format("No possible way to resolve path {} and no default resolver set", path));

            result = m_defaultResolver(path);
        }

        if (result.isOk()) {
            const auto [it, inserted] = m_sourceContainer.insert_or_assign(path, result.unwrap());
            return Result::good(&it->second);
        }

        return Result::err(result.unwrapErrs());
    }
}
