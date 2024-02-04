#pragma once

#include <optional>
#include <vector>
#include <pl/core/errors/error.hpp>

namespace pl::hlp {

    template <typename Ok, typename Err>
    struct Result {
        std::optional<Ok> ok;
        std::vector<Err> errs;

        Result() = default;

        explicit Result(const Ok& ok) : ok(ok), errs({ }) { }
        Result(const Ok& ok, const std::vector<Err>& errs) : ok(ok), errs(errs) { }
        Result(const Ok& ok, const Err& err) : ok(ok), errs({ err }) { }
        Result(Result&& other) noexcept : ok(other.ok), errs(other.errs) { }
        Result(const Result& other) noexcept : ok(other.ok), errs(other.errs) { }
        Result(std::optional<Ok> ok, const std::vector<Err>& errs) : ok(ok), errs(errs) { }
        // move assignment operator

        Result& operator=(Result&& other) noexcept {
            this->ok = other.ok;
            this->errs = other.errs;
            return *this;
        }

        static Result good(const Ok& ok) {
            return Result(ok);
        }

        static Result err(const Err& err) {
            return { std::nullopt, { err } };
        }

        static Result err(const std::vector<Err>& errs) {
            return { std::nullopt, errs };
        }

        [[nodiscard]] bool isOk() const {
            return ok.has_value();
        }

        [[nodiscard]] bool isErr() const {
            return !isOk();
        }

        [[nodiscard]] bool hasErrs() const {
            return !errs.empty();
        }

        const Ok& unwrap() const {
            return ok.value();
        }

        const std::vector<Err>& unwrapErrs() const {
            return errs;
        }

        Ok& unwrap() {
            return ok.value();
        }

        std::vector<Err>& unwrapErrs() {
            return errs;
        }

    };

    template <typename T>
    using CompileResult = hlp::Result<T, pl::core::err::CompileError>;

}