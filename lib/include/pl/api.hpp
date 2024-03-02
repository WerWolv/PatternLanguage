#pragma once

#include <pl/core/token.hpp>
#include <pl/core/errors/result.hpp>
#include <pl/helpers/types.hpp>

#include <cmath>
#include <vector>
#include <functional>
#include <string>
#include <optional>

namespace pl {

    class PatternLanguage;

    namespace core {
        class Evaluator;
        class Preprocessor;
    }

}

namespace pl::api {

    /**
     * @brief A pragma handler is a function that is called when a pragma is encountered.
     */
    using PragmaHandler = std::function<bool(PatternLanguage&, const std::string &)>;

    using DirectiveHandler = std::function<void(core::Preprocessor*, u32)>;

    using Resolver = std::function<hlp::Result<Source*, std::string>(const std::string&)>;

    struct Source {
        std::string content;
        std::string source;
        u32 id = 0;

        static u32 idCounter;

        Source(std::string content, std::string source = DefaultSource) :
            content(std::move(content)), source(std::move(source)), id(idCounter++) { }

        Source() = default;

        static constexpr auto DefaultSource = "<Source Code>";
        static constexpr Source* NoSource = nullptr;
        static Source Empty() {
            return { "", "" };
        }

        constexpr auto operator<=>(const Source& other) const {
            return this->id <=> other.id;
        }

    };

    /**
     * @brief A type representing a custom section
     */
    struct Section {
        std::string name;
        std::vector<u8> data;
    };

    /**
     * @brief Type to pass to function register functions to specify the number of parameters a function takes.
     */
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

    /**
     * @brief A type representing a namespace.
     */
    using Namespace = std::vector<std::string>;

    /**
     * @brief A function callback called when a function is called.
     */
    using FunctionCallback  = std::function<std::optional<core::Token::Literal>(core::Evaluator *, const std::vector<core::Token::Literal> &)>;

    /**
     * @brief A type representing a function.
     */
    struct Function {
        FunctionParameterCount parameterCount;
        std::vector<core::Token::Literal> defaultParameters;
        FunctionCallback func;
        bool dangerous;
    };

}