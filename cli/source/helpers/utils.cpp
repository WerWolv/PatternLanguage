#include <cli/helpers/utils.hpp>

#include <wolv/io/file.hpp>

namespace pl::cli {

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

        runtime.setDataSource(baseAddress, inputFile.getSize(), [&](u64 address, void *buffer, size_t size) {
            inputFile.seek(address - baseAddress);
            inputFile.readBuffer(static_cast<u8*>(buffer), size);
        });

        // Execute pattern file
        if (!runtime.executeString(patternFile.readString())) {
            auto error = runtime.getError().value();
            fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
            std::exit(EXIT_FAILURE);
        }
    }

}