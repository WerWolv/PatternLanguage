#include "pl/cli/helpers/utils.hpp"
#include "pl/pattern_language.hpp"

#include <pl/cli/cli.hpp>

#include <wolv/io/file.hpp>

#include <CLI/App.hpp>

#include <fmt/format.h>

// Available subcommands
namespace pl::cli {

    namespace sub {

        void addFormatSubcommand(CLI::App *app, pl::PatternLanguage &runtime);
        void addRunSubcommand(CLI::App *app, pl::PatternLanguage &runtime);
        void addDocsSubcommand(CLI::App *app, pl::PatternLanguage &runtime);
        void addInfoSubcommand(CLI::App *app, pl::PatternLanguage &runtime);

    }

    // Run the pattern language CLI
    // first argument (args[0]) is the subcommand, not the executable name
    int executeCommandLineInterface(std::vector<std::string> args, pl::PatternLanguage &runtime) {
        CLI::App app("Pattern Language CLI");
        app.require_subcommand();

        // Add subcommands
        sub::addFormatSubcommand(&app, runtime);
        sub::addRunSubcommand(&app, runtime);
        sub::addDocsSubcommand(&app, runtime);
        sub::addInfoSubcommand(&app, runtime);

        // Print help message if not enough arguments were provided
        if (args.empty()) {
            fmt::print("{}", app.help());
            return EXIT_FAILURE;
        } else if (args.size() == 1) {
            std::string subcommand = args[0];
            if (subcommand == "-h" || subcommand == "--help") {
                fmt::print("{}", app.help());
                return EXIT_FAILURE;
            } else if (subcommand == "--version") {
                fmt::print("{}", "v1.0.0");
                return EXIT_SUCCESS;
            }

            try {
                fmt::print("{}\n", app.get_subcommand(subcommand)->help());
            } catch (CLI::OptionNotFound &) {
                fmt::print("Invalid subcommand '{}'\n", subcommand);
            }

            return EXIT_FAILURE;
        }

        // Parse command line input
        try {
            std::reverse(args.begin(), args.end()); // wanted by CLI11
            app.parse(args);
        } catch(const CLI::ParseError &e) {
            return app.exit(e, std::cout, std::cout);
        } catch (const ExitException &e) {
            return e.exitCode;
        }

        return EXIT_SUCCESS;
    }

}

#if defined (LIBPL_CLI_AS_EXECUTABLE)
    int main(int argc, char** argv) {
        std::vector<std::string> args;
        for (int i = 1; i < argc; i++) {
            args.push_back(argv[i]);
        }

        pl::PatternLanguage runtime;
        return pl::cli::executeCommandLineInterface(args, runtime);
    }
#endif