#include <pl/pattern_language.hpp>
#include <pl/core/parser.hpp>
#include <wolv/io/file.hpp>

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_union.hpp>

namespace pl::cli::sub {

    namespace {

        std::string trimValue(const std::string &string) {
            std::string trimmed = wolv::util::trim(string);

            if (trimmed.starts_with('"') && trimmed.ends_with('"'))
                trimmed = trimmed.substr(1, trimmed.size() - 2);

            return trimmed;
        }

    }

    void addInfoSubcommand(CLI::App *app) {
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static std::fs::path patternFilePath;
        static std::string type;

        auto subcommand = app->add_subcommand("info");

        // Add command line arguments
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
        subcommand->add_option("-t,--type", type, "Type of information you want to query")->required()->check([](const std::string &value) -> std::string {
            if (value == "name" || value == "authors" || value == "description" || value == "mime" || value == "version")
                return "";
            else
                return "Invalid type. Valid types are: [ name, authors, description, mime, version ]";
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

            std::string patternName, patternVersion;
            std::vector<std::string> patternAuthors, patternDescriptions, patternMimes;

            runtime.addPragma("name", [&](auto &, const std::string &value) -> bool {
                patternName = trimValue(value);
                return true;
            });

            runtime.addPragma("author", [&](auto &, const std::string &value) -> bool {
                patternAuthors.push_back(trimValue(value));
                return true;
            });

            runtime.addPragma("description", [&](auto &, const std::string &value) -> bool {
                patternDescriptions.push_back(trimValue(value));
                return true;
            });

            runtime.addPragma("MIME", [&](auto &, const std::string &value) -> bool {
                patternMimes.push_back(trimValue(value));
                return true;
            });

            runtime.addPragma("version", [&](auto &, const std::string &value) -> bool {
                patternVersion = trimValue(value);
                return true;
            });

            auto ast = runtime.parseString(patternFile.readString());
            if (!ast.has_value()) {
                auto error = runtime.getError().value();
                fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
                std::exit(EXIT_FAILURE);
            }

            if (type == "name") {
                if (!patternName.empty())
                    fmt::print("{}\n", patternName);
            } else if (type == "authors") {
                for (const auto &author : patternAuthors) {
                    fmt::print("{}\n", author);
                }
            } else if (type == "description") {
                for (const auto &description : patternDescriptions) {
                    fmt::print("{}\n", description);
                }
            } else if (type == "mime") {
                for (const auto &mime : patternMimes) {
                    fmt::print("{}\n", mime);
                }
            } else if (type == "version") {
                if (!patternVersion.empty())
                    fmt::print("{}\n", patternVersion);
            }

        });
    }

}