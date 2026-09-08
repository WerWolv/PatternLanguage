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

        explicit Result(Ok ok) : ok(std::move(ok)), errs({ }) { }
        Result(Ok ok, std::vector<Err> errs) : ok(std::move(ok)), errs(std::move(errs)) { }
        Result(Ok ok, Err err) : ok(std::move(ok)), errs({ std::move(err) }) { }
        Result(Result&& other) noexcept : ok(other.ok), errs(other.errs) { } // why copy?
        Result(const Result& other) noexcept : ok(other.ok), errs(other.errs) { }
        Result(std::optional<Ok> ok, std::vector<Err> errs) : ok(std::move(ok)), errs(std::move(errs)) { }
        // move assignment operator

        // why copy?
        Result& operator=(Result&& other) noexcept {
            this->ok = other.ok;
            this->errs = other.errs;
            return *this;
        }

        static Result good(Ok ok) {
            return Result(std::move(ok));
        }

        static Result err(Err err) {
            return { std::nullopt, { std::move(err) } };
        }

        static Result err(std::vector<Err> errs) {
            return { std::nullopt, std::move(errs) };
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