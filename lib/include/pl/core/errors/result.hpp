#pragma once

#include <string>
#include <optional>
#include <vector>

namespace pl::hlp {

    template <typename Ok, typename Err>
    struct Result {
        std::optional<Ok> ok;
        std::vector<Err> errs;

        Result() = default;

        Result(const Ok& ok, const std::vector<Err>& errs) : ok(ok), errs(errs) { }
        Result(const Result&& other) noexcept : ok(other.ok), errs(other.errs) { }

        [[nodiscard]] bool is_ok() const {
            return ok.has_value();
        }

        [[nodiscard]] bool is_err() const {
            return !is_ok();
        }

        [[nodiscard]] bool has_errs() const {
            return !errs.empty();
        }

        const Ok& unwrap() const {
            return ok.value();
        }

        const std::vector<Err>& unwrap_errs() const {
            return errs;
        }

        Ok& unwrap() {
            return ok.value();
        }

        std::vector<Err>& unwrap_errs() {
            return errs;
        }

    };

}