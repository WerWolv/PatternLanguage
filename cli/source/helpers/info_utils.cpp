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

    nlohmann::json runSingleFile(pl::PatternLanguage &runtime, const std::string &patternFilePath) {
        // reset pragma handlers
        for (const auto &pragma : runtime.getPragmas()) {
            // runtime.removePragma(pragma.first);
        }

        // Setup pragma handlers

        std::string patternName, patternVersion;
        std::vector<std::string> patternAuthors, patternDescriptions, patternMimes;

        runtime.addPragma("name", [&](auto &, const std::string &value) -> bool {
            patternName = trimValue(value);
            return true;
        });

        runtime.addPragma("author", [&](auto &, const std::string &value) -> bool {
            patternAuthors.push_back(trimValue(value));
            return true;
        });

        runtime.addPragma("description", [&](auto &, const std::string &value) -> bool {
            patternDescriptions.push_back(trimValue(value));
            return true;
        });

        runtime.addPragma("MIME", [&](auto &, const std::string &value) -> bool {
            patternMimes.push_back(trimValue(value));
            return true;
        });

        runtime.addPragma("version", [&](auto &, const std::string &value) -> bool {
            patternVersion = trimValue(value);
            return true;
        });

        // Open pattern file
        wolv::io::File patternFile(patternFilePath, wolv::io::File::Mode::Read);

        // Run pattern file
        auto ast = runtime.parseString(patternFile.readString(), patternFilePath);
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
            std::exit(EXIT_FAILURE);
        }

        return nlohmann::json {
            {"name", patternName},
            {"authors", patternAuthors},
            {"description", wolv::util::combineStrings(patternDescriptions, ".\n")},
            {"MIMEs", patternMimes},
            {"version", patternVersion},
        };
    }
}
