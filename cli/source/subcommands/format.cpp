#include <pl/pattern_language.hpp>
#include <pl/formatters.hpp>
#include <wolv/io/file.hpp>

#include <cli/helpers/utils.hpp>

#include <CLI/CLI.hpp>
#include <fmt/format.h>

namespace pl::cli::sub {

    void addFormatSubcommand(CLI::App *app) {
        static const auto formatters = pl::gen::fmt::createFormatters();

        static std::fs::path inputFilePath, outputFilePath, patternFilePath;
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static std::string formatterName;
        static bool verbose = false;
        static bool allowDangerousFunctions = false;
        static bool metaInformation = false;
        static u64 baseAddress = 0x00;

        auto subcommand = app->add_subcommand("format");

        // Add command line arguments
        subcommand->add_option("-i,--input,INPUT_FILE", inputFilePath, "Input file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-o,--output,OUTPUT_FILE", outputFilePath, "Output file")->check(CLI::NonexistentPath);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
        subcommand->add_option("-b,--base", baseAddress, "Base address")->default_val(0x00);
        subcommand->add_flag("-v,--verbose", verbose, "Verbose output")->default_val(false);
        subcommand->add_flag("-d,--dangerous", allowDangerousFunctions, "Allow dangerous functions")->default_val(false);
        subcommand->add_flag("-m,--metadata", metaInformation, "Include meta type information")->default_val(0x00);
        subcommand->add_option("-f,--formatter", formatterName, "Formatter")->default_val("default")->check([&](const auto &value) -> std::string {
            // Validate if the selected formatter exists
            if (std::any_of(formatters.begin(), formatters.end(), [&](const auto &formatter) { return formatter->getName() == value; }))
                return "";
            else {
                std::vector<std::string> formatterNames;
                for (const auto &formatter : formatters)
                    formatterNames.push_back(formatter->getName());

                return ::fmt::format("Invalid formatter. Valid formatters are: [{}]", ::fmt::join(formatterNames, ", "));
            }
        });

        // If no formatter has been selected, select the first one
        if (formatterName == "default")
            formatterName = formatters.front()->getName();

        subcommand->callback([] {
            // Find formatter that matches this name
            const auto &formatter = *std::find_if(formatters.begin(), formatters.end(),
                                                  [&](const auto &formatter) {
                                                      return formatter->getName() == formatterName;
                                                  });

            // If no output path was given, use the input path with the formatter's file extension
            if (outputFilePath.empty()) {
                outputFilePath = inputFilePath;
                outputFilePath.replace_extension(formatter->getFileExtension());
            }

            // Open input file
            wolv::io::File inputFile(inputFilePath, wolv::io::File::Mode::Read);
            if (!inputFile.isValid()) {
                ::fmt::print("Failed to open file '{}'\n", inputFilePath.string());
                std::exit(EXIT_FAILURE);
            }

            // Open pattern file
            wolv::io::File patternFile(patternFilePath, wolv::io::File::Mode::Read);
            if (!patternFile.isValid()) {
                ::fmt::print("Failed to open file '{}'\n", patternFilePath.string());
                std::exit(EXIT_FAILURE);
            }

            // Create and configure Pattern Language runtime
            pl::PatternLanguage runtime;
            pl::cli::executePattern(runtime, inputFile, patternFile, includePaths, defines, allowDangerousFunctions, baseAddress);

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

            // Set formatter settings
            formatter->enableMetaInformation(metaInformation);

            // Call selected formatter to format the results
            auto result = formatter->format(runtime);

            // Write results to output file
            wolv::io::File outputFile(outputFilePath, wolv::io::File::Mode::Create);
            if (!outputFile.isValid()) {
                ::fmt::print("Failed to create output file: {}\n", outputFilePath.string());
                std::exit(EXIT_FAILURE);
            }

            outputFile.writeVector(result);
        });
    }

}