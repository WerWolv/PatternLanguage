#include <pl/pattern_language.hpp>
#include <pl/formatters.hpp>
#include <wolv/io/file.hpp>

#include <pl/cli/helpers/utils.hpp>

#include <CLI/CLI.hpp>
#include <CLI/App.hpp>
#include <fmt/format.h>

#include <nlohmann/json.hpp>

namespace pl::cli::sub {

    void addLintSubcommand(CLI::App *app) {
        static std::fs::path patternFilePath;
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static bool outputJson = false;

        auto subcommand = app->add_subcommand("lint", "Compiles the given pattern");

        // Add command line arguments
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
        subcommand->add_flag("-j,--json", outputJson, "Output JSON instead of text")->default_val(false);

        subcommand->callback([] {
            // Open pattern file
            wolv::io::File patternFile(patternFilePath, wolv::io::File::Mode::Read);
            if (!patternFile.isValid()) {
                ::fmt::print("Failed to open file '{}'\n", patternFilePath.string());
                std::exit(EXIT_FAILURE);
            }

            // Create and configure Pattern Language runtime
            pl::PatternLanguage runtime;

            runtime.setIncludePaths(includePaths);

            for (const auto &define : defines)
                runtime.addDefine(define);

            const auto code = patternFile.readString();
            const auto source = wolv::util::toUTF8String(patternFile.getPath());
            auto _ = runtime.parseString(code, source);

            auto compileErrors = runtime.getCompileErrors();
            ::nlohmann::json errors = ::nlohmann::json::array();
            if (compileErrors.size() > 0) {
                for (const auto &error : compileErrors) {
                    if (outputJson) {
                        ::nlohmann::json obj;
                        obj["message"] = error.getMessage();
                        obj["description"] = error.getDescription();

                        ::nlohmann::json loc;
                        const auto &location = error.getLocation();
                        loc["line"] = location.line;
                        loc["column"] = location.column;
                        loc["source"] = location.source->source;
                        obj["location"] = loc;

                        errors.emplace_back(obj);
                    } else {
                        ::fmt::print("{}\n", error.format());
                    }
                }
            }

            if(outputJson) {
                ::fmt::print("{}\n", errors.dump());
            }
        });
    }

}
