#pragma once

#include <string>
#include <optional>
#include <vector>

namespace pl {

    template <typename Ok, typename Err>
    struct Result {
        std::optional<Ok> ok;
        std::vector<Err> errs;

        Result() = default;

        Result(const Ok& ok) : ok(ok) {}
        Result(Ok&& ok) : ok(std::move(ok)) {}

        Result(const Err& err) : errs({err}) {}
        Result(Err&& err) : errs({std::move(err)}) {}

        Result(const std::vector<Err>& errs) : errs(errs) {}
        Result(std::vector<Err>&& errs) : errs(std::move(errs)) {}

        Result(const std::optional<Ok>& ok, const std::vector<Err>& errs) : ok(ok), errs(errs) {}
        Result(const std::optional<Ok>& ok, std::vector<Err>&& errs) : ok(ok), errs(std::move(errs)) {}

        Result(std::optional<Ok>&& ok, const std::vector<Err>& errs) : ok(std::move(ok)), errs(errs) {}

        bool is_ok() const {
            return ok.has_value();
        }

        bool is_err() const {
            return !is_ok();
        }

        bool has_errs() const {
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

    class Error {
    public:
        explicit Error(std::string message) : m_message(std::move(message)) {}

        [[nodiscard]] const std::string& get_message() const {
            return m_message;
        }

    private:
        std::string m_message;
    };

}