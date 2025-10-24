#pragma once

#include <pl/helpers/types.hpp>
#include <pl/core/location.hpp>

#include <string>
#include <utility>
#include <vector>

namespace pl::core::err {

    namespace impl {
        std::string formatRuntimeError(
                const Location &location,
                const std::string &message,
                const std::string &description,
                std::optional<u64> address = std::nullopt);

        std::string formatRuntimeErrorShort(
                const std::string &message,
                const std::string &description);

        std::string formatCompilerError(
            const Location &location,
            const std::string &message,
            const std::string &description,
            const std::vector<Location> &trace);

        std::string formatLocation(Location location, std::optional<u64> address = std::nullopt);
        std::string formatLines(Location location);
    }

    template<typename T = void>
    class UserData {
    public:
        UserData() = default;
        UserData(const T &userData) : m_userData(userData) { }
        UserData(const UserData &) = default;
        UserData(UserData &) = default;
        UserData(UserData &&) = default;
        UserData(auto && ... args) : m_userData(std::forward<decltype(args)>(args)...) { }

        const T& getUserData() const { return this->m_userData; }

    private:
        T m_userData = { };
    };

    struct PatternLanguageError : std::exception {
        PatternLanguageError(std::string message, u32 line, u32 column, std::optional<u64> cursorAddress = std::nullopt) : message(std::move(message)), line(line), column(column), cursorAddress(cursorAddress) { }

        std::string message;
        u32 line, column;
        std::optional<u64> cursorAddress;

        [[nodiscard]] const char *what() const noexcept override {
            return this->message.c_str();
        }
    };

    template<>
    class UserData<void> { };

    template<typename T = void>
    class RuntimeError {
    public:
        class Exception : public std::exception, public UserData<T> {
        public:
            Exception(u32 errorCode, std::string title, std::string description, std::string hint, UserData<T> userData = {}) :
                    UserData<T>(userData), m_errorCode(errorCode), m_title(std::move(title)), m_description(std::move(description)), m_hint(std::move(hint)) {
                this->m_shortMessage = impl::formatRuntimeErrorShort(this->m_description, this->m_hint);
            }

            [[nodiscard]] const char *what() const noexcept override {
                return this->m_shortMessage.c_str();
            }

            [[nodiscard]] std::string format(const Location& location) const {
                return impl::formatRuntimeError(location, this->m_description, this->m_hint);
            }

        private:
            u32 m_errorCode;
            std::string m_shortMessage;
            std::string m_title, m_description, m_hint;
        };

        RuntimeError(u32 errorCode, std::string title) : m_errorCode(errorCode), m_title(std::move(title)) {

        }

        [[nodiscard]] std::string format(const std::string &description, const std::string &hint = { }, UserData<T> userData = { }) const {
            return Exception(this->m_errorCode, this->m_title, description, hint, std::move(userData)).what();
        }

        [[noreturn]] void throwError(const std::string &description, const std::string &hint = { }, UserData<T> userData = { }) const {
            throw Exception(this->m_errorCode, this->m_title, description, hint, std::move(userData));
        }

    private:
        char m_prefix = 'E';
        u32 m_errorCode;
        std::string m_title;
    };

    class CompileError {

    public:
        CompileError(std::string message, const Location& location) : m_message(std::move(message)), m_location(location) { }
        CompileError(std::string message, std::string description, const Location& location) : m_message(std::move(message)), m_description(std::move(description)),
                                                                                              m_location(location) { }

        [[nodiscard]] const std::string& getMessage() const { return this->m_message; }
        [[nodiscard]] const std::string& getDescription() const { return this->m_description; }
        [[nodiscard]] const Location& getLocation() const { return this->m_location; }
        [[nodiscard]] Location& getLocation() { return this->m_location; }
        [[nodiscard]] const std::vector<Location>& getTrace() const { return this->m_trace; }
        [[nodiscard]] std::vector<Location>& getTrace() { return this->m_trace; }

        [[nodiscard]] std::string format() const {
            return impl::formatCompilerError(this->getLocation(), this->getMessage(), this->getDescription(), getTrace());
        }

    private:
        std::string m_message;
        std::string m_description;
        Location m_location;
        std::vector<Location> m_trace;
    };

    class ErrorCollector {
    public:

        virtual ~ErrorCollector() = default;

        virtual Location location() = 0;

        void error(const std::string &message) {
            this->m_errors.emplace_back(message, location());
        }

        void errorDesc(const std::string &message, const std::string &description) {
            this->m_errors.emplace_back(message, description, location());
        }

        void error(CompileError& error) {
            error.getTrace().push_back(location());
            this->m_errors.push_back(std::move(error));
        }

        void errorAt(const Location& location, const std::string &message) {
            this->m_errors.emplace_back(message, location);
        }

        void errorAtDesc(const Location& location, const std::string &message, const std::string &description) {
            this->m_errors.emplace_back(message, description, location);
        }

        [[nodiscard]] bool hasErrors() const {
            return !this->m_errors.empty();
        }

        [[nodiscard]] const std::vector<CompileError>& getErrors() const {
            return this->m_errors;
        }

        [[nodiscard]] std::vector<CompileError> collectErrors() {
            return std::move(this->m_errors);
        }

        void clear() {
            this->m_errors.clear();
        }
    private:
        std::vector<CompileError> m_errors;
    };

}