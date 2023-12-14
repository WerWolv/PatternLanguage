#include <pl/core/resolvers.hpp>

#include <wolv/io/file.hpp>

using namespace pl::core;
using namespace pl::api;
using namespace pl::hlp;

Result<Source, std::string> FileResolver::resolve(const std::string &path) const {
    using result_t = Result<Source, std::string>;
    if (const auto it = this->m_virtualFiles.find(path); it != this->m_virtualFiles.end()) {
        return result_t::good(it->second);
    }

    const std::fs::path fsPath(path);

    for (const auto &item: this->m_includePaths) {
        if(auto fullPath = item / fsPath; std::fs::exists(fullPath)) {

            auto file = wolv::io::File(fullPath.string(), wolv::io::File::Mode::Read);

            if(!file.isValid()) {
                return result_t::err("Could not open file " + fullPath.string());
            }

            if(!std::fs::is_regular_file(fullPath)) {
                return result_t::err("Path " + fullPath.string() + " is not a regular file");
            }

            const auto content = file.readString();

            return result_t::good(Source { content, fullPath.string() });
        }
    }

    return result_t::err("Could not find file " + path);
}
