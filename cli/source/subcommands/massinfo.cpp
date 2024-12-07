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

    void addMassInfoSubcommand(CLI::App *app) {
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static std::fs::path patternsFolderPath;
        static std::string type;
        static std::string formatterName;

        auto subcommand = app->add_subcommand("massinfo", "Print all information in JSON about a folder of patterns");

        // Add command line arguments
        subcommand->add_option("-p,--pattern", patternsFolderPath, "Pattern folder")->required()->check(CLI::ExistingDirectory);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
        
        subcommand->callback([] {

            // Create and configure Pattern Language runtime
            pl::PatternLanguage runtime;
            runtime.setDangerousFunctionCallHandler([&]() {
                return false;
            });

            for (const auto &define : defines)
                runtime.addDefine(define);

            runtime.setIncludePaths(includePaths);

            // Recursively search for all pattern files
            std::vector<std::fs::path> patternFiles;
            for (const auto &entry : std::fs::recursive_directory_iterator(patternsFolderPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                    patternFiles.push_back(entry.path());
                }
            }

            // run files and put output in JSON
            nlohmann::json json = {};
            int successParses = 0;
            for (const auto &patternFile : patternFiles) {
                std::string relativeFilePath = patternFile.string().substr(patternsFolderPath.string().size() + 1);
                json[relativeFilePath] = runSingleFile(runtime, patternFile);
                successParses++;
            }

            fmt::print("{}\n", json.dump());
            fmt::print(stderr, "Processed {}/{} files successfully\n", successParses, patternFiles.size());
        });
    }

}