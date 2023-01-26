#pragma once

#include <pl/pattern_language.hpp>
#include <pl/helpers/file.hpp>

#include <vector>

namespace pl::cli {

    void executePattern(
            PatternLanguage &runtime,
            pl::hlp::fs::File &inputFilePath,
            pl::hlp::fs::File &patternFilePath,
            const std::vector<std::fs::path> &includePaths,
            const std::vector<std::string> &defines,
            bool allowDangerousFunctions,
            u64 baseAddress);
}