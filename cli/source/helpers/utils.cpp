#include <pl/cli/helpers/utils.hpp>

#include <wolv/io/file.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <pl/helpers/utils.hpp>
#include <wolv/utils/string.hpp>

namespace pl::cli {

    static std::vector<pl::u8> parseByteString(const std::string &string) {
        auto byteString = std::string(string);
        std::erase(byteString, ' ');

        if ((byteString.length() % 2) != 0) return {};

        std::vector<pl::u8> result;
        for (pl::u32 i = 0; i < byteString.length(); i += 2) {
            if (!std::isxdigit(byteString[i]) || !std::isxdigit(byteString[i + 1]))
                return {};

            result.push_back(std::strtoul(byteString.substr(i, 2).c_str(), nullptr, 16));
        }

        return result;
    }

    void executePattern(
            PatternLanguage &runtime,
            wolv::io::File &inputFile,
            wolv::io::File &patternFile,
            const std::vector<std::fs::path> &includePaths,
            const std::vector<std::string> &defines,
            bool allowDangerousFunctions,
            u64 baseAddress) {

        runtime.setDangerousFunctionCallHandler([&]() {
            return allowDangerousFunctions;
        });

        runtime.setIncludePaths(includePaths);

        for (const auto &define : defines)
            runtime.addDefine(define);

        if (inputFile.isValid()) {
            // Include baseAddress as a copy to prevent it from going out of scope in the lambda
            runtime.setDataSource(baseAddress, inputFile.getSize(), [&inputFile, baseAddress](u64 address, void *buffer, size_t size) {
                inputFile.seek(address - baseAddress);
                inputFile.readBuffer(static_cast<u8*>(buffer), size);
            });
        } else {
            runtime.addPragma("example", [](pl::PatternLanguage &runtime, const std::string &value) {
                auto data = parseByteString(value);
                runtime.setDataSource(0, data.size(), [data](pl::u64 address, pl::u8 *buffer, pl::u64 size) {
                    std::memcpy(buffer, data.data() + address, size);
                });

                return true;
            });
        }

        // Execute pattern file
        if (!runtime.executeString(patternFile.readString(), wolv::util::toUTF8String(patternFile.getPath()))) {
            auto compileErrors = runtime.getCompileErrors();
            if (compileErrors.size()>0) {
                fmt::print("Compilation failed\n");
                for (const auto &error : compileErrors) {
                    fmt::print("{}\n", error.format());
                }
            } else {
                auto error = runtime.getEvalError().value();
                fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
            }
            std::exit(EXIT_FAILURE);
        }
    }

}
