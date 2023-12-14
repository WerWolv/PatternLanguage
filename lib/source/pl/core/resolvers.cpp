#include <pl/core/resolvers.hpp>

#include <wolv/io/file.hpp>

using namespace pl::api;
using namespace pl::hlp;

namespace pl::core {
    FileResolver::result FileResolver::resolve(const std::string &path) const {
        if (const auto it = this->m_virtualFiles.find(path); it != this->m_virtualFiles.end()) {
            return result::good(it->second);
        }

        const std::fs::path fsPath(path);

        for (const auto &item: this->m_includePaths) {
            if(auto fullPath = item / fsPath; std::fs::exists(fullPath)) {

                auto file = wolv::io::File(fullPath.string(), wolv::io::File::Mode::Read);

                if(!file.isValid()) {
                    return result::err("Could not open file " + fullPath.string());
                }

                if(!std::fs::is_regular_file(fullPath)) {
                    return result::err("Path " + fullPath.string() + " is not a regular file");
                }

                const auto content = file.readString();

                return result::good(Source { content, fullPath.string() });
            }
        }

        return result::err("Could not find file " + path);
    }
}
