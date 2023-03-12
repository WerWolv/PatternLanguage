#include <wolv/io/file.hpp>

#include <CLI/CLI.hpp>

#include <fmt/format.h>

// Available subcommands
namespace pl::cli::sub {

    void addFormatSubcommand(CLI::App *app);
    void addRunSubcommand(CLI::App *app);
    void addDocsSubcommand(CLI::App *app);
    void addInfoSubcommand(CLI::App *app);

}

int main(int argc, char** argv) {
    CLI::App app("Pattern Language CLI");
    app.require_subcommand();

    // Add subcommands
    pl::cli::sub::addFormatSubcommand(&app);
    pl::cli::sub::addRunSubcommand(&app);
    pl::cli::sub::addDocsSubcommand(&app);
    pl::cli::sub::addInfoSubcommand(&app);

    // Print help message if not enough arguments were provided
    if (argc == 1) {
        fmt::print("{}", app.help());
        return EXIT_FAILURE;
    } else if (argc == 2) {
        std::string subcommand = argv[1];
        if (subcommand == "-h" || subcommand == "--help") {
            fmt::print("{}", app.help());
            return EXIT_FAILURE;
        }

        try {
            fmt::print("{}\n", app.get_subcommand(subcommand)->help());
        } catch (CLI::OptionNotFound &) {
            fmt::print("Invalid subcommand '{}'\n", subcommand);
        }

        return EXIT_FAILURE;
    }

    // Parse command line input
    CLI11_PARSE(app, argc, argv);

    return EXIT_SUCCESS;
}