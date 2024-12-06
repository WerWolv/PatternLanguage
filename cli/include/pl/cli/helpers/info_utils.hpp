#pragma once

#include <pl/pattern_language.hpp>

#include <nlohmann/json.hpp>

namespace pl::cli {
    std::string trimValue(const std::string &string);

    nlohmann::json runSingleFile(pl::PatternLanguage &runtime, const std::string &filepath);    
}