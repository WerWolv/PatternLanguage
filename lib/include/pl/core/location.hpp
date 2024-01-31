#pragma once

#include <pl/helpers/types.hpp>

namespace pl::api {
    struct Source;
}

namespace pl::core {

    struct Location {

        const api::Source* source;
        u32 line, column;
        size_t length;

        constexpr static Location Empty() {
            return { nullptr, 0, 0, 0 };
        }

    };

}