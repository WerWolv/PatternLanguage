#include <pl/pattern_language.hpp>
#include <pl/codegens.hpp>
#include <pl/helpers/file.hpp>

#include <cli/helpers/utils.hpp>

#include <CLI/CLI.hpp>
#include <fmt/format.h>

namespace pl::cli::sub {

    void addCodeGenSubcommand(CLI::App *app) {
        static const auto generators = pl::gen::code::createCodeGenerators();

        static std::fs::path outputFilePath, patternFilePath;
        static std::vector<std::fs::path> includePaths;

        static std::string generatorName;
        static bool verbose = false;
        static bool allowDangerousFunctions = false;

        auto subcommand = app->add_subcommand("codegen");

        // Add command line arguments
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-o,--output,OUTPUT_FILE", outputFilePath, "Output file")->check(CLI::NonexistentPath);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_flag("-v,--verbose", verbose, "Verbose output")->default_val(false);
        subcommand->add_flag("-d,--dangerous", allowDangerousFunctions, "Allow dangerous functions")->default_val(false);
        subcommand->add_option("-g,--generator", generatorName, "Generator")->default_val("default")->check([&](const auto &value) -> std::string {
            // Validate if the selected generator exists
            if (std::any_of(generators.begin(), generators.end(), [&](const auto &generator) { return generator->getName() == value; }))
                return "";
            else {
                std::vector<std::string> generatorNames;
                for (const auto &generator : generators)
                    generatorNames.push_back(generator->getName());

                return ::fmt::format("Invalid generator. Valid generators are: [{}]", ::fmt::join(generatorNames, ", "));
            }
        });

        // If no generator has been selected, select the first one
        if (generatorName == "default")
            generatorName = generators.front()->getName();

        subcommand->callback([] {
            // Find generator that matches this name
            const auto &generator = *std::find_if(generators.begin(), generators.end(),
                                                  [&](const auto &generator) {
                                                      return generator->getName() == generatorName;
                                                  });

            // Open pattern file
            hlp::fs::File patternFile(patternFilePath, hlp::fs::File::Mode::Read);
            if (!patternFile.isValid()) {
                ::fmt::print("Failed to open file '{}'\n", patternFilePath.string());
                std::exit(EXIT_FAILURE);
            }

            if (outputFilePath.empty()) {
                outputFilePath = patternFilePath;
                outputFilePath.replace_extension(generator->getFileExtension());
            }

            // Create and configure Pattern Language runtime
            pl::PatternLanguage runtime;
            auto ast = pl::cli::parsePattern(runtime, patternFile, includePaths, allowDangerousFunctions);

            // Output console log if verbose mode is enabled
            if (verbose) {
                for (const auto &[level, message] : runtime.getConsoleLog()) {
                    switch (level) {
                        using enum pl::core::LogConsole::Level;

                        case Debug:
                            ::fmt::print("[DEBUG] {}\n", message);
                            break;
                        case Info:
                            ::fmt::print("[INFO]  {}\n", message);
                            break;
                        case Warning:
                            ::fmt::print("[WARN]  {}\n", message);
                            break;
                        case Error:
                            ::fmt::print("[ERROR] {}\n", message);
                            break;
                    }
                }
            }

            // Call selected generator to format the results
            auto result = generator->generate(runtime, ast);

            // Write results to output file
            hlp::fs::File outputFile(outputFilePath, hlp::fs::File::Mode::Create);
            if (!outputFile.isValid()) {
                ::fmt::print("Failed to create output file: {}\n", outputFilePath.string());
                std::exit(EXIT_FAILURE);
            }

            outputFile.write(result);
        });
    }

}