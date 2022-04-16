#pragma once

#include <helpers/types.hpp>

#include <pl/token.hpp>

#include <cmath>
#include <vector>
#include <functional>
#include <string>
#include <optional>

namespace pl {

    class Evaluator;

    struct BuiltinFunctionParameterCount {
        BuiltinFunctionParameterCount() = default;

        constexpr bool operator==(const BuiltinFunctionParameterCount &other) const {
            return this->min == other.min && this->max == other.max;
        }

        [[nodiscard]] static BuiltinFunctionParameterCount unlimited() {
            return BuiltinFunctionParameterCount { 0, 0xFFFF'FFFF };
        }

        [[nodiscard]] static BuiltinFunctionParameterCount none() {
            return BuiltinFunctionParameterCount { 0, 0 };
        }

        [[nodiscard]] static BuiltinFunctionParameterCount exactly(u32 value) {
            return BuiltinFunctionParameterCount { value, value };
        }

        [[nodiscard]] static BuiltinFunctionParameterCount moreThan(u32 value) {
            return BuiltinFunctionParameterCount { value + 1, 0xFFFF'FFFF };
        }

        [[nodiscard]] static BuiltinFunctionParameterCount lessThan(u32 value) {
            return BuiltinFunctionParameterCount { 0, u32(std::max<i64>(i64(value) - 1, 0)) };
        }

        [[nodiscard]] static BuiltinFunctionParameterCount atLeast(u32 value) {
            return BuiltinFunctionParameterCount { value, 0xFFFF'FFFF };
        }

        [[nodiscard]] static BuiltinFunctionParameterCount between(u32 min, u32 max) {
            return BuiltinFunctionParameterCount { min, max };
        }

        u32 min = 0, max = 0;
    private:
        BuiltinFunctionParameterCount(u32 min, u32 max) : min(min), max(max) { }
    };

    using Namespace = std::vector<std::string>;
    using BuiltinFunctionCallback  = std::function<std::optional<pl::Token::Literal>(pl::Evaluator *, const std::vector<pl::Token::Literal> &)>;

    struct BuiltinFunction {
        BuiltinFunctionParameterCount parameterCount;
        std::vector<pl::Token::Literal> defaultParameters;
        BuiltinFunctionCallback func;
        bool dangerous;
    };

}