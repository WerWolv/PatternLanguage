#pragma once

#include "pl/helpers/types.hpp"

#include <string>

namespace pl::core {

    struct Location {

        std::string source;
        u32 line, column;

    };

    static constexpr Location EmptyLocation = { "", 0, 0 };

}