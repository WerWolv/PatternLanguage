#pragma once

#include <string>

#include <pl/helpers/types.hpp>

namespace pl::core {

    // The encoding a string pattern resolved to on the last successful run. Plain data, not a
    // reference to the pattern. See PatternLanguage::getStringEncodingRegions().
    struct StringEncodingRegion {
        u64 section;
        u64 address;
        u64 size;
        std::string encoding;
    };

}
