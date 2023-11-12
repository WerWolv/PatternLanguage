#pragma once

#include "pl/helpers/types.hpp"

#include <string>

namespace pl::api {
    struct Source;
}

namespace pl::core {

    struct Location {

        api::Source* source;
        u32 line, column;
        size_t length;

    };

    static constexpr api::Source* EmptySource = nullptr;
    static constexpr Location EmptyLocation = { EmptySource, 0, 0, 0 };
    static constexpr std::string DefaultSource = "<Source Code>";

}