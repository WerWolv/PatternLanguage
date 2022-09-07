#include <pl.hpp>
#include <pl/helpers/file.hpp>
#include <pl/formatters.hpp>

#include <CLI/CLI.hpp>

#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/ranges.h>

using namespace pl;

template<size_t N = 0>
auto createFormatters(std::array<std::unique_ptr<pl::cli::Formatter>, std::tuple_size_v<Formatters>> &&result = {}) {
    auto formatter = std::unique_ptr<pl::cli::Formatter>(new typename std::tuple_element<N, Formatters>::type());

    result[N] = std::move(formatter);

    if constexpr (N + 1 < std::tuple_size_v<Formatters>) {
        return createFormatters<N + 1>(std::move(result));
    } else {
        return result;
    }
}

static bool ignorePragma(PatternLanguage&, const std::string &) { return true; }

int main(int argc, char** argv) {
    CLI::App app("Pattern Language CLI");

    const auto formatters = createFormatters();

    std::fs::path inputFilePath, outputFilePath, patternFilePath;
    std::vector<std::fs::path> includePaths;

    std::string formatterName;
    bool verbose = false;
    bool allowDangerousFunctions = false;
    u64 baseAddress = 0x00;

    // Add command line arguments
    app.add_option("-i,--input,INPUT_FILE", inputFilePath, "Input file")->required()->check(CLI::ExistingFile);
    app.add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output,OUTPUT_FILE", outputFilePath, "Output file")->check(CLI::NonexistentPath);
    app.add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
    app.add_option("-v,--verbose", verbose, "Verbose output")->default_val(false);
    app.add_option("-d,--dangerous", allowDangerousFunctions, "Allow dangerous functions")->default_val(false);
    app.add_option("-b,--base", baseAddress, "Base address")->default_val(0x00);
    app.add_option("-f,--formatter", formatterName, "Formatter")->default_val("default")->check([&](const auto &value) -> std::string {
        // Validate if the selected formatter exists
        if (std::any_of(formatters.begin(), formatters.end(), [&](const auto &formatter) { return formatter->getName() == value; }))
            return "";
        else {
            std::vector<std::string> formatterNames;
            for (const auto &formatter : formatters)
                formatterNames.push_back(formatter->getName());

            return fmt::format("Invalid formatter. Valid formatters are: [{}]", fmt::join(formatterNames, ", "));
        }
    });

    // If no formatter has been selected, select the first one
    if (formatterName == "default")
        formatterName = formatters.front()->getName();

    // Find formatter that matches this name
    const auto &formatter = *std::find_if(formatters.begin(), formatters.end(),
        [&](const auto &formatter) {
            return formatter->getName() == formatterName;
        });

    // Print help message if no arguments were given
    if (argc == 1) {
        fmt::print("{}", app.help());
        return EXIT_FAILURE;
    }

    CLI11_PARSE(app, argc, argv);


    {
        // If no output path was given, use the input path with the formatter's file extension
        if (outputFilePath.empty()) {
            outputFilePath = inputFilePath;
            outputFilePath.replace_extension(formatter->getFileExtension());
        }

        // Create and configure Pattern Language runtime
        PatternLanguage runtime;

        runtime.setDangerousFunctionCallHandler([&]() {
            return allowDangerousFunctions;
        });

        runtime.addPragma("MIME", ignorePragma);

        runtime.setIncludePaths(includePaths);

        // Use input file as data source
        hlp::fs::File inputFile(inputFilePath, hlp::fs::File::Mode::Read);
        if (!inputFile.isValid()) {
            fmt::print("Failed to open file '{}'\n", inputFilePath.string());
            return EXIT_FAILURE;
        }

        runtime.setDataSource([&](u64 address, void *buffer, size_t size) {
            inputFile.seek(address - baseAddress);
            inputFile.readBuffer(static_cast<u8*>(buffer), size);
        }, baseAddress, inputFile.getSize());

        // Execute pattern file
        if (!runtime.executeFile(patternFilePath)) {
            auto error = runtime.getError().value();
            fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
            return EXIT_FAILURE;
        }

        // Output console log if verbose mode is enabled
        if (verbose) {
            for (const auto &[level, message] : runtime.getConsoleLog()) {
                switch (level) {
                    using enum pl::core::LogConsole::Level;

                    case Debug:
                        fmt::print("[DEBUG] {}\n", message);
                        break;
                    case Info:
                        fmt::print("[INFO]  {}\n", message);
                        break;
                    case Warning:
                        fmt::print("[WARN]  {}\n", message);
                        break;
                    case Error:
                        fmt::print("[ERROR] {}\n", message);
                        break;
                }
            }
        }

        // Call selected formatter to format the results
        auto result = formatter->format(runtime.getAllPatterns());

        // Write results to output file
        hlp::fs::File outputFile(outputFilePath, hlp::fs::File::Mode::Create);
        if (!outputFile.isValid()) {
            fmt::print("Failed to create output file: {}\n", outputFilePath.string());
            return EXIT_FAILURE;
        }

        outputFile.write(result);
    }

    return EXIT_SUCCESS;
}