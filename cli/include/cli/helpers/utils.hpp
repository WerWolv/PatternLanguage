#pragma once

#include <pl/pattern_language.hpp>
#include <pl/helpers/file.hpp>

namespace pl::cli {

    void executePattern(
            PatternLanguage &runtime,
            pl::hlp::fs::File &inputFilePath,
            pl::hlp::fs::File &patternFilePath,
            const std::vector<std::fs::path> &includePaths,
            bool allowDangerousFunctions,
            u64 baseAddress);

    std::vector<std::shared_ptr<pl::core::ast::ASTNode>> parsePattern(
            PatternLanguage &runtime,
            pl::hlp::fs::File &patternFile,
            const std::vector<std::fs::path> &includePaths,
            bool allowDangerousFunctions);

}