#pragma once

#include <pl/formatters/formatter.hpp>
#include <pl/formatters/formatter_json.hpp>
#include <pl/formatters/formatter_yaml.hpp>
#include <pl/formatters/formatter_html.hpp>

namespace pl::gen::fmt {

    // Available formatters. Add new ones here to make them available
    using Formatters = std::tuple<
            FormatterJson,
            FormatterYaml,
            FormatterHtml
    >;

    using FormatterArray = std::array<std::unique_ptr<pl::gen::fmt::Formatter>, std::tuple_size_v<Formatters>>;

    template<size_t N = 0>
    auto createFormatters(FormatterArray &&result = {}) {
        auto formatter = std::unique_ptr<pl::gen::fmt::Formatter>(new typename std::tuple_element<N, Formatters>::type());

        result[N] = std::move(formatter);

        if constexpr (N + 1 < std::tuple_size_v<Formatters>) {
            return createFormatters<N + 1>(std::move(result));
        } else {
            return result;
        }
    }

}