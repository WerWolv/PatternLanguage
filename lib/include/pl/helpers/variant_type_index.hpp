#pragma once

#include <variant>
#include <type_traits>
#include <cstddef>

namespace pl::hlp {

template<typename T, typename Variant>
struct VariantTypeIndex;

template<typename T, typename... Ts>
struct VariantTypeIndex<T, std::variant<Ts...>> {
    static constexpr auto value = [] -> std::size_t {
        static_assert(
            (std::is_same_v<std::remove_cvref_t<T>, Ts> || ...),
            "T not in std::variant");
        constexpr bool matches[] = { std::is_same_v<std::remove_cvref_t<T>, Ts>... };

        for (std::size_t i=0; i<sizeof...(Ts); ++i) {
            if (matches[i]) { return i; }
        }

        return -1;
    }();
};

template<class T, class Variant>
inline constexpr std::size_t variantTypeIndex_v = VariantTypeIndex<T, Variant>::value;

} // namespace pl::hlp