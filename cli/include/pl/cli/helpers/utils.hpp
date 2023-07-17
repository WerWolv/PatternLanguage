#pragma once

#include <pl/pattern_language.hpp>
#include <wolv/io/file.hpp>

#include <vector>

namespace pl::cli {

    void executePattern(
            PatternLanguage &runtime,
            wolv::io::File &inputFilePath,
            wolv::io::File &patternFilePath,
            const std::vector<std::fs::path> &includePaths,
            const std::vector<std::string> &defines,
            bool allowDangerousFunctions,
            u64 baseAddress);
}