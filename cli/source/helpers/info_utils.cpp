#include <pl/cli/helpers/info_utils.hpp>

#include <wolv/utils/string.hpp>
#include <wolv/io/file.hpp>

namespace pl::cli {

    std::string trimValue(const std::string &string) {
        std::string trimmed = wolv::util::trim(string);

        if (trimmed.starts_with('"') && trimmed.ends_with('"'))
            trimmed = trimmed.substr(1, trimmed.size() - 2);

        return trimmed;
    }

    void clearPragmas(pl::PatternLanguage &runtime) {
        std::vector<std::string> pragmas;
        for (const auto &pragma : runtime.getPragmas()) {
            pragmas.push_back(pragma.first);
        }
        for (const auto &pragma : pragmas) {
            runtime.removePragma(pragma);
        }
    }

    std::vector<std::string> getMultimapValues(std::multimap<std::string, std::string> &pragmas, const std::string key) {
        std::vector<std::string> values;
        auto [begin, end] = pragmas.equal_range(key);
        for (auto it = begin; it != end; ++it) {
            values.push_back(trimValue(it->second));
        }
        return values;
    }

    std::string getLastOrFallback(const std::vector<std::string> &values, const std::string &fallback) {
        if (values.empty()) {
            return fallback;
        }
        return values.back();
    }

    std::optional<PatternMetadata> parsePatternMetadata(pl::PatternLanguage &runtime, const std::string &patternData) {
        clearPragmas(runtime);

        auto pragmaValues = runtime.getPragmaValues(patternData);

        PatternMetadata metadata = {};

        metadata.name = getLastOrFallback(getMultimapValues(pragmaValues, "name"), "");
        metadata.authors = getMultimapValues(pragmaValues, "author");
        metadata.mimes = getMultimapValues(pragmaValues, "mime");
        metadata.version = getLastOrFallback(getMultimapValues(pragmaValues, "version"), "");
        metadata.description = wolv::util::combineStrings(getMultimapValues(pragmaValues, "description"), "\n");

        for (const auto &[key, value] : pragmaValues) {
            metadata.pragmas[key].push_back(trimValue(value));
        }

        return metadata;
    }

    nlohmann::json PatternMetadata::toJSON() {
        return {
            { "name",           name        },
            { "description",    description },
            { "authors",        authors     },
            { "mimes",          mimes       },
            { "version",        version     },
            { "pragmas",        pragmas     }
        };
    }
}
