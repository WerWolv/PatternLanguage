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

    // Helper method
    void printPatternFilePlain(const pl::cli::PatternMetadata &metadata) {
        fmt::print("Pattern name: {}\n", metadata.name);
        fmt::print("Authors: {}\n", wolv::util::combineStrings(metadata.authors, ", "));
        fmt::print("Description: {}\n", metadata.description);
        fmt::print("MIMEs: {}\n", wolv::util::combineStrings(metadata.mimes, ", "));
        fmt::print("Version: {}\n", metadata.version);
    }

    // Logic for --formatter plain -p
    void outputPatternFilePlain(pl::PatternLanguage &runtime, const std::string &filePath) {
        wolv::io::File patternFile(filePath, wolv::io::File::Mode::Read);
        auto metadata_opt = parsePatternMetadata(runtime, patternFile.readString());
        if (metadata_opt.has_value()) {
            printPatternFilePlain(metadata_opt.value());
        } else {
            fmt::print(stderr, "Error parsing file: {}\n", filePath);
        }
    }

    // Logic for --formatter json -p
    void outputPatternFileJSON(pl::PatternLanguage &runtime, const std::string &filePath) {
        wolv::io::File patternFile(filePath, wolv::io::File::Mode::Read);
        auto metadata_opt = parsePatternMetadata(runtime, patternFile.readString());
        if (metadata_opt.has_value()) {
            fmt::print("{}\n", metadata_opt.value().toJSON().dump());
        } else {
            fmt::print(stderr, "Error parsing file: {}\n", filePath);
        }
    }

    // Logic for --formatter plain -P
    void outputPatternDirectoryPlain(pl::PatternLanguage &runtime, const std::string &directoryPath) {
        for (const auto &entry : std::fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                auto file = wolv::io::File(entry.path(), wolv::io::File::Mode::Read);
                auto metadata_opt = parsePatternMetadata(runtime, file.readString());
                if (metadata_opt.has_value()) {
                    printPatternFilePlain(metadata_opt.value());
                } else {
                    fmt::print(stderr, "Error parsing file: {}\n", entry.path().filename().string());
                }
            }
        }
    }

    // Logic for --formatter json -P
    void outputPatternDirectoryJSON(pl::PatternLanguage &runtime, const std::string &directoryPath) {
        nlohmann::json json = {};
        for (const auto &entry : std::fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                auto file = wolv::io::File(entry.path(), wolv::io::File::Mode::Read);
                auto metadata_opt = parsePatternMetadata(runtime, file.readString());
                if (metadata_opt.has_value()) {
                    json[entry.path().filename().string()] = metadata_opt.value().toJSON();
                } else {
                    fmt::print(stderr, "Error parsing file: {}\n", entry.path().filename().string());
                }
            }
        }
        fmt::print("{}\n", json.dump());
    }

    void addInfoSubcommand(CLI::App *app) {
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static std::fs::path patternFilePath;
        static std::fs::path patternDirPath;
        static std::string formatterName;

        auto subcommand = app->add_subcommand("info", "Print information about a pattern");

        // Add command line arguments
        auto exclusive_group = subcommand->add_option_group("Pattern Options", "Only one pattern option can be used");
        exclusive_group->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->check(CLI::ExistingFile);
        exclusive_group->add_option("-P,--pattern-dir,PATTERN_DIR", patternDirPath, "Pattern directory")->check(CLI::ExistingDirectory);
        exclusive_group->require_option(1); // Require exactly one of these

        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
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

           // Execute correct logic method
           if (formatterName == "pretty") {
               if (patternFilePath.empty()) {
                   outputPatternDirectoryPlain(runtime, wolv::util::toUTF8String(patternDirPath));
               } else {
                   outputPatternFilePlain(runtime, wolv::util::toUTF8String(patternFilePath));
               }
           } else if (formatterName == "json") {
               if (patternFilePath.empty()) {
                   outputPatternDirectoryJSON(runtime, wolv::util::toUTF8String(patternDirPath));
               } else {
                   outputPatternFileJSON(runtime, wolv::util::toUTF8String(patternFilePath));
               }
           } else {
             throw std::runtime_error("SHOULD NOT HAPPEN: Invalid formatter");
           }

        });
    }

}