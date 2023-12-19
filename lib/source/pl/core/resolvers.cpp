#include <pl/core/resolvers.hpp>

#include <wolv/io/file.hpp>

namespace pl::core::resolvers {
    Result FileResolver::resolve(const std::string &path) const {
        if (const auto it = this->m_virtualFiles.find(path); it != this->m_virtualFiles.end()) {
            return Result::good(it->second);
        }

        const std::fs::path fsPath(path);

        for (const auto &item: this->m_includePaths) {
            if(auto fullPath = item / fsPath; std::fs::exists(fullPath)) {

                auto file = wolv::io::File(fullPath.string(), wolv::io::File::Mode::Read);

                if(!file.isValid()) {
                    return Result::err("Could not open file " + fullPath.string());
                }

                if(!std::fs::is_regular_file(fullPath)) {
                    return Result::err("Path " + fullPath.string() + " is not a regular file");
                }

                const auto content = file.readString();

                return Result::good(api::Source { content, fullPath.string() });
            }
        }

        return Result::err("Could not find file " + path);
    }
}
