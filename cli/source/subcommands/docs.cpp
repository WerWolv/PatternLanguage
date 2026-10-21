#include <pl/cli/subcommands/docs.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/parser.hpp>
#include <pl/cli/helpers/utils.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

#include <CLI/CLI.hpp>
#include <CLI/App.hpp>
#include <fmt/format.h>

#include <pl/core/ast/ast_node_function_definition.hpp>

namespace pl::cli::sub {

    void addDocsSubcommand(CLI::App *app, pl::PatternLanguage &runtime) {
        static std::vector<std::fs::path> includePaths;
        static std::vector<std::string> defines;

        static std::fs::path patternFilePath, outputFilePath;
        static bool hideImplementationDetails;

        auto subcommand = app->add_subcommand("docs");

        // Add command line arguments
        subcommand->add_option("-p,--pattern,PATTERN_FILE", patternFilePath, "Pattern file")->required()->check(CLI::ExistingFile);
        subcommand->add_option("-o,--output,OUTPUT_FILE", outputFilePath, "Output file")->required();
        subcommand->add_option("-I,--includes", includePaths, "Include file paths")->take_all()->check(CLI::ExistingDirectory);
        subcommand->add_option("-D,--define", defines, "Define a preprocessor macro")->take_all();
        subcommand->add_flag("-n,--noimpls", hideImplementationDetails, "Hide implementation details");

        subcommand->callback([&runtime] {

            // Configure Pattern Language runtime
            runtime.setDangerousFunctionCallHandler([&]() {
                return false;
            });

            for (const auto &define : defines)
                runtime.addDefine(define);

            runtime.setIncludePaths(includePaths);

            // Execute pattern file
            wolv::io::File patternFile(patternFilePath, wolv::io::File::Mode::Read);

            auto ast = runtime.parseString(patternFile.readString(), wolv::util::toUTF8String(patternFile.getPath()));
            if (!ast.has_value()) {
                auto compileErrors = runtime.getCompileErrors();
                if (!compileErrors.empty()) {
                    fmt::print("Compilation failed\n");
                    for (const auto &error : compileErrors) {
                        fmt::print("{}\n", error.format());
                    }
                } else {
                    auto error = runtime.getEvalError().value();
                    fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
                }

                throw ExitException(EXIT_FAILURE);
            }

            // Output documentation
            std::string documentation = fmt::format("# {}\n", patternFilePath.stem().string());
            {
                // Add global documentation
                for (auto comment : runtime.getInternals().parser->getGlobalDocComments()) {
                    comment = wolv::util::trim(comment);
                    if (comment.starts_with('*'))
                        comment = comment.substr(1);

                    for (const auto &line : wolv::util::splitString(comment, "\n"))
                        documentation += fmt::format("{}\n", wolv::util::trim(line));
                }

                {
                    std::string sectionContent;
                    for (const auto &[name, type] : runtime.getInternals().parser->getTypes()) {
                        if (!type->shouldDocument())
                            continue;
                        if (hideImplementationDetails && name.contains("impl::"))
                            continue;

                        auto sourceLocation = type->getLocation();
                        if (sourceLocation.source == nullptr || std::filesystem::absolute(sourceLocation.source->source) != std::filesystem::absolute(patternFilePath))
                            continue;

                        sectionContent += fmt::format("### `{}`\n", name);

                        for (auto line : wolv::util::splitString(type->getDocComment(), "\n")) {
                            line = wolv::util::trim(line);
                            if (line.starts_with('*'))
                                line = line.substr(1);
                            line = wolv::util::trim(line);

                            if (line.starts_with('@')) {
                                line = line.substr(1);

                                if (line.starts_with("tparam ")) {
                                    line = line.substr(6);
                                    line = wolv::util::trim(line);

                                    if (line.empty())
                                        continue;

                                    auto paramName = wolv::util::splitString(line, " ")[0];
                                    sectionContent += fmt::format("- `<{}>`: {}\n", paramName, wolv::util::trim(line.substr(paramName.size())));
                                } else if (line.starts_with("internal ")) {
                                    goto skip_type;
                                }
                            } else {
                                sectionContent += line + "\n";
                            }
                        }

                        sectionContent += docs::generateTypeDocumentation(wolv::util::splitString(name, "::").back(), type.get()) + "\n";

                        skip_type:;
                    }

                    if (!sectionContent.empty()) {
                        documentation += "\n\n## Types\n\n";
                        documentation += sectionContent;
                    }
                }


                {
                    std::string sectionContent;

                    for (const auto &node : *ast) {
                        if (!node->getLocation().source->mainSource)
                            continue;

                        if (!node->shouldDocument())
                            continue;

                        if (auto *functionDecl = dynamic_cast<core::ast::ASTNodeFunctionDefinition *>(node.get()); functionDecl != nullptr) {
                            const auto &name = functionDecl->getName();
                            if (hideImplementationDetails && name.contains("impl::"))
                                continue;

                            sectionContent += fmt::format("### `{}`\n", name);

                            for (auto line : wolv::util::splitString(functionDecl->getDocComment(), "\n")) {
                                line = wolv::util::trim(line);
                                if (line.starts_with('*'))
                                    line = line.substr(1);
                                line = wolv::util::trim(line);

                                if (line.starts_with('@')) {
                                    line = line.substr(1);

                                    if (line.starts_with("param ")) {
                                        line = line.substr(5);
                                        line = wolv::util::trim(line);

                                        if (line.empty())
                                            continue;

                                        auto paramName = wolv::util::splitString(line, " ")[0];
                                        sectionContent += fmt::format("- `{}`: {}\n", paramName, wolv::util::trim(line.substr(paramName.size())));
                                    } else if (line.starts_with("return ")) {
                                        line = line.substr(6);
                                        line = wolv::util::trim(line);

                                        sectionContent += fmt::format("- `return`: {}\n", line);
                                    } else if (line.starts_with("internal ")) {
                                        goto skip_function;
                                    }
                                } else {
                                    sectionContent += line + "\n";
                                }
                            }

                            sectionContent += "\n```pl\n";
                            sectionContent += fmt::format("fn {}(", wolv::util::splitString(functionDecl->getName(), "::").back());

                            const auto &params = functionDecl->getParams();
                            for (const auto &[paramName, paramType] : params) {
                                std::string typeName = docs::getTypeName(paramType.get());

                                sectionContent += fmt::format("{} {}, ", typeName, paramName);
                            }

                            if (auto paramPack = functionDecl->getParameterPack(); paramPack.has_value()) {
                                sectionContent += fmt::format("auto ... {}, ", paramPack.value());
                            }

                            if (!params.empty()) {
                                sectionContent.pop_back();
                                sectionContent.pop_back();
                            }

                            sectionContent += ");\n```\n\n";
                        }

                        skip_function:;
                    }

                    if (!sectionContent.empty()) {
                        documentation += "\n\n## Functions\n\n";
                        documentation += sectionContent;
                    }
                }
            }

            wolv::io::fs::createDirectories(outputFilePath.parent_path());
            wolv::io::File outputFile(outputFilePath, wolv::io::File::Mode::Create);
            outputFile.writeString(documentation);
        });
    }

}
