#include <pl/pattern_language.hpp>
#include <pl/core/parser.hpp>
#include <pl/cli/helpers/info_utils.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

#include <CLI/CLI.hpp>
#include <CLI/App.hpp>
#include <fmt/format.h>

#include <nlohmann/json.hpp>

namespace pl::cli::sub {

    void addInfoSubcommand(CLI::App *app) {
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static std::fs::path patternFilePath;
        static std::string type;
        static std::string formatterName;

        auto subcommand = app->add_subcommand("info", "Print information about a pattern");

        // Add command line arguments
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
        subcommand->add_option("-t,--type", type, "Type of information you want to query")->check([](const std::string &value) -> std::string {
            if (value == "name" || value == "authors" || value == "description" || value == "mime" || value == "version")
                return "";
            else
                return "Invalid type. Valid types are: [ name, authors, description, mime, version ]";
        });
        subcommand->add_option("-f,--formatter", formatterName, "Formatter")->default_val("pretty")->check([&](const auto &value) -> std::string {
            if (value == "pretty" || value == "json")
                return "";
            else
                return "Invalid formatter. Valid formatters are: [pretty, json]";
        });

        subcommand->callback([] {

            // Create and configure Pattern Language runtime
            pl::PatternLanguage runtime;
            runtime.setDangerousFunctionCallHandler([&]() {
                return false;
            });

            for (const auto &define : defines)
                runtime.addDefine(define);

            runtime.setIncludePaths(includePaths);

            // Execute pattern file
            wolv::io::File patternFile(patternFilePath, wolv::io::File::Mode::Read);
            auto metadata = parsePatternMetadata(runtime, patternFile.readString());

            if (formatterName == "json") {
                if(!type.empty()) {
                    fmt::print("Error: --type is not compatible with --format json\n");
                    std::exit(EXIT_FAILURE);
                }

                fmt::print("{}\n", metadata.toJSON().dump());
            } else if (formatterName == "pretty") {
                if (type.empty()) {
                        fmt::print("Pattern name: {}\n", metadata.name);
                        fmt::print("Authors: {}\n", wolv::util::combineStrings(metadata.authors, ", "));
                        fmt::print("Description: {}\n", metadata.description);
                        fmt::print("MIMEs: {}\n", wolv::util::combineStrings(metadata.mimes, ", "));
                        fmt::print("Version: {}\n", metadata.version);
                } else if (type == "name") {
                    if (!metadata.name.empty())
                        fmt::print("{}\n", metadata.name);
                } else if (type == "authors") {
                    for (const auto &author : metadata.authors) {
                        fmt::print("{}\n", author);
                    }
                } else if (type == "description") {
                    fmt::print("{}\n", metadata.description);
                } else if (type == "mime") {
                    for (const auto &mime : metadata.mimes) {
                        fmt::print("{}\n", mime);
                    }
                } else if (type == "version") {
                    if (!metadata.version.empty())
                        fmt::print("{}\n", metadata.version);
                }
            }

        });
    }

}