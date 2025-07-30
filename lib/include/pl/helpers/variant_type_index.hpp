#pragma once

#include <variant>
#include <type_traits>
#include <cstddef>

namespace pl::hlp {

template<typename T, typename Variant>
struct variant_type_index;

template<typename T, typename... Ts>
struct variant_type_index<T, std::variant<Ts...>> {
    static constexpr std::size_t value = []{
        constexpr bool matches[] = { std::is_same_v<std::remove_cvref_t<T>, Ts>... };

        std::size_t idx = 0;
        for (std::size_t i = 0; i < sizeof...(Ts); ++i) {
            if (matches[i]) { idx = i; }
        }

        return idx;
    }();
};

template<class T, class Variant>
inline constexpr std::size_t variant_type_index_v = variant_type_index<T, Variant>::value;

} // namespace pl::hlp