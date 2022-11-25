#include <pl/pattern_language.hpp>
#include <pl/formatters.hpp>
#include <pl/helpers/file.hpp>

#include <CLI/CLI.hpp>
#include <fmt/format.h>

namespace pl::cli::sub {

    void addRunSubcommand(CLI::App *app) {
        static std::vector<std::fs::path> includePaths;

        static std::string formatterName;
        static bool verbose = false;
        static bool allowDangerousFunctions = false;
        static u64 baseAddress = 0x00;

        static std::fs::path inputFilePath, patternFilePath;

        auto subcommand = app->add_subcommand("run");

        // Add command line arguments
        subcommand->add_option("-i,--input,INPUT_FILE", inputFilePath, "Input file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-b,--base", baseAddress, "Base address")->default_val(0x00);
        subcommand->add_flag("-v,--verbose", verbose, "Verbose output")->default_val(false);
        subcommand->add_flag("-d,--dangerous", allowDangerousFunctions, "Allow dangerous functions")->default_val(false);

        subcommand->callback([] {

            // Create and configure Pattern Language runtime
            pl::PatternLanguage runtime;
            runtime.setDangerousFunctionCallHandler([&]() {
                return allowDangerousFunctions;
            });

            runtime.addPragma("MIME", [](auto&, const auto&){ return true; });

            runtime.setIncludePaths(includePaths);

            auto data = hlp::fs::File(inputFilePath, hlp::fs::File::Mode::Read).readBytes();
            runtime.setDataSource([&](u64 address, void *buffer, size_t size) {
                if (address + size > data.size())
                    std::memset(buffer, 0x00, size);
                else
                    std::memcpy(buffer, data.data() + address, size);
            }, baseAddress, data.size());

            // Execute pattern file
            if (!runtime.executeFile(patternFilePath)) {
                auto error = runtime.getError().value();
                fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
                std::exit(EXIT_FAILURE);
            }

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
        });
    }

}