#pragma once

#include <pl/pattern_language.hpp>

#include <nlohmann/json.hpp>

namespace pl::cli {
    std::string trimValue(const std::string &string);

    struct PatternMetadata {
        std::string name;
        std::string description;
        std::vector<std::string> authors;
        std::vector<std::string> mimes;
        std::string version;
        
        nlohmann::json toJSON();
    };


    
    PatternMetadata parsePatternMetadata(pl::PatternLanguage &runtime, const std::string &patternData);
}