#pragma once

#include <pl/helpers/types.hpp>
#include <optional>

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

        bool operator==(const Location& other) const {
            if (source != other.source)
                return false;
            return line == other.line && column == other.column;
        }

        bool operator!=(const Location& other) const {
            return !(*this == other);
        }

        std::optional<bool> operator<(const Location& other) const {
            if (source != other.source)
                return std::nullopt;
            return  (line < other.line) || ( line == other.line && column < other.column);
        }

        std::optional<bool> operator>(const Location& other) const {
            if (source != other.source)
                return std::nullopt;
            return  (line > other.line) || (line == other.line && column > other.column);
        }

        std::optional<bool> operator<=(const Location& other) const {
            if (*this == other)
                return true;
            return (*this < other);
        }

        std::optional<bool> operator>=(const Location& other) const {
            if (*this == other)
                return true;
            return (*this > other);
        }

    };

}