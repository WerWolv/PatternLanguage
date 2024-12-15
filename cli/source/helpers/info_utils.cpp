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

    std::optional<PatternMetadata> parsePatternMetadata(pl::PatternLanguage &runtime, const std::string &patternData) {
        // reset pragma handlers
        clearPragmas(runtime);

        // Setup pragma handlers
        PatternMetadata metadata;
        std::vector<std::string> descriptions;
        runtime.addPragma("name", [&](auto &, const std::string &value) -> bool {
            metadata.name = trimValue(value);
            return true;
        });

        runtime.addPragma("author", [&](auto &, const std::string &value) -> bool {
            metadata.authors.push_back(trimValue(value));
            return true;
        });

        runtime.addPragma("description", [&](auto &, const std::string &value) -> bool {
            descriptions.push_back(trimValue(value));
            return true;
        });

        runtime.addPragma("MIME", [&](auto &, const std::string &value) -> bool {
            metadata.mimes.push_back(trimValue(value));
            return true;
        });

        runtime.addPragma("version", [&](auto &, const std::string &value) -> bool {
            metadata.version = trimValue(value);
            return true;
        });

        // Run pattern file
        auto ast = runtime.parseString(patternData, "pattern.hexpat");
        if (!ast.has_value()) {
            auto compileErrors = runtime.getCompileErrors();
            if (compileErrors.size()>0) {
                fmt::print("Compilation failed\n");
                for (const auto &error : compileErrors) {
                    fmt::print("{}\n", error.format());
                }
            } else {
                auto error = runtime.getEvalError().value();
                fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
            }
            return {};
        }

        // Set description
        metadata.description = wolv::util::combineStrings(descriptions, "\n");

        return metadata;
    }

    nlohmann::json PatternMetadata::toJSON() {
        return {
            {"name", name},
            {"description", description},
            {"authors", authors},
            {"mimes", mimes},
            {"version", version}
        };
    }
}
