#include <cli/helpers/utils.hpp>

#include <pl/helpers/file.hpp>

namespace pl::cli {

    static bool ignorePragma(PatternLanguage&, const std::string &) { return true; }

    void executePattern(
            PatternLanguage &runtime,
            pl::hlp::fs::File &inputFile,
            pl::hlp::fs::File &patternFile,
            const std::vector<std::fs::path> &includePaths,
            bool allowDangerousFunctions,
            u64 baseAddress) {

        runtime.setDangerousFunctionCallHandler([&]() {
            return allowDangerousFunctions;
        });

        runtime.addPragma("MIME", ignorePragma);

        runtime.setIncludePaths(includePaths);

        runtime.setDataSource([&](u64 address, void *buffer, size_t size) {
            inputFile.seek(address - baseAddress);
            inputFile.readBuffer(static_cast<u8*>(buffer), size);
        }, baseAddress, inputFile.getSize());

        // Execute pattern file
        if (!runtime.executeString(patternFile.readString())) {
            auto error = runtime.getError().value();
            fmt::print("Pattern Error: {}:{} -> {}\n", error.line, error.column, error.message);
            std::exit(EXIT_FAILURE);
        }
    }

}