#pragma once

#include <pl/formatters/formatter.hpp>
#include <pl/formatters/formatter_json.hpp>

// Available formatters. Add new ones here to make them available
using Formatters = std::tuple<
        pl::cli::FormatterJson
>;