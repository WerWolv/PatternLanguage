#include <pl/core/resolvers.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace pl::core::resolvers {
    Result FileResolver::resolve(const std::string &path) const {
        if (const auto it = this->m_virtualFiles.find(path); it != this->m_virtualFiles.end()) {
            return Result::good(it->second);
        }

        const std::fs::path fsPath(path);

        for (const auto &item: this->m_includePaths) {
            auto fullPath = item / fsPath;

            bool exists = false;
            // if the path has no extension
            if (!fullPath.has_extension()) {
                for (const auto &ext : { "hexpat", "pat" }) {
                    fullPath.replace_extension(ext);
                    exists = std::fs::exists(fullPath);
                    if (exists) {
                        break;
                    }
                }
            } else {
                exists = std::fs::exists(fullPath);
            }

            if(!exists)
                continue;

            const auto utf8 = wolv::util::toUTF8String(fullPath);
            auto file = wolv::io::File(utf8, wolv::io::File::Mode::Read);

            if(!file.isValid()) {
                return Result::err("Could not open file " + utf8);
            }

            if(!std::fs::is_regular_file(fullPath)) {
                return Result::err("Path " + utf8 + " is not a regular file");
            }

            const auto content = file.readString();

            return Result::good(api::Source { content, utf8 });
        }

        return Result::err("Could not find file " + path);
    }
}
