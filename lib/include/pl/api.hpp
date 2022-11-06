#pragma once

#include <pl/core/token.hpp>
#include <pl/helpers/types.hpp>

#include <cmath>
#include <vector>
#include <functional>
#include <string>
#include <optional>

namespace pl {

    class PatternLanguage;

    namespace core { class Evaluator; }

}

namespace pl::api {

    using PragmaHandler = std::function<bool(PatternLanguage&, const std::string &)>;

    struct Section {
        std::string name;
        std::vector<u8> data;
    };

    struct FunctionParameterCount {
        FunctionParameterCount() = default;

        constexpr bool operator==(const FunctionParameterCount &other) const {
            return this->min == other.min && this->max == other.max;
        }

        [[nodiscard]] static FunctionParameterCount unlimited() {
            return FunctionParameterCount { 0, 0xFFFF'FFFF };
        }

        [[nodiscard]] static FunctionParameterCount none() {
            return FunctionParameterCount { 0, 0 };
        }

        [[nodiscard]] static FunctionParameterCount exactly(u32 value) {
            return FunctionParameterCount { value, value };
        }

        [[nodiscard]] static FunctionParameterCount moreThan(u32 value) {
            return FunctionParameterCount { value + 1, 0xFFFF'FFFF };
        }

        [[nodiscard]] static FunctionParameterCount lessThan(u32 value) {
            return FunctionParameterCount { 0, u32(std::max<i64>(i64(value) - 1, 0)) };
        }

        [[nodiscard]] static FunctionParameterCount atLeast(u32 value) {
            return FunctionParameterCount { value, 0xFFFF'FFFF };
        }

        [[nodiscard]] static FunctionParameterCount between(u32 min, u32 max) {
            return FunctionParameterCount { min, max };
        }

        u32 min = 0, max = 0;
    private:
        FunctionParameterCount(u32 min, u32 max) : min(min), max(max) { }
    };

    using Namespace = std::vector<std::string>;
    using FunctionCallback  = std::function<std::optional<core::Token::Literal>(core::Evaluator *, const std::vector<core::Token::Literal> &)>;

    struct Function {
        FunctionParameterCount parameterCount;
        std::vector<core::Token::Literal> defaultParameters;
        FunctionCallback func;
        bool dangerous;
    };

}