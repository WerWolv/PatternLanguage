#pragma once

#include <pl/helpers/types.hpp>
#include "pl/core/location.hpp"
#include <pl/core/errors/result.hpp>

#include <fmt/format.h>

#include <string>
#include <stdexcept>
#include <utility>
#include <vector>
#include <functional>

namespace pl::core::err {

    std::string formatImpl(
            const std::string &sourceCode,
            u32 line, u32 column,
            char prefix,
            u32 errorCode,
            const std::string &title,
            const std::string &description,
            const std::string &hint);

    std::string formatShortImpl(
            char prefix,
            u32 errorCode,
            const std::string &title,
            const std::string &description);

    template<typename T = void>
    class UserData {
    public:
        UserData() = default;
        UserData(const T &userData) : m_userData(userData) { }
        UserData(const UserData<T> &) = default;

        const T& getUserData() const { return this->m_userData; }

    private:
        T m_userData = { };
    };

    struct PatternLanguageError : public std::exception {
        PatternLanguageError(std::string message, u32 line, u32 column) : message(std::move(message)), line(line), column(column) { }

        std::string message;
        u32 line, column;

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
            Exception(char prefix, u32 errorCode, std::string title, std::string description, std::string hint, UserData<T> userData = {}) :
                    UserData<T>(userData), m_prefix(prefix), m_errorCode(errorCode), m_title(std::move(title)), m_description(std::move(description)), m_hint(std::move(hint)) {
                this->m_shortMessage = formatShortImpl(this->m_prefix, this->m_errorCode, this->m_title, this->m_description);
            }

            [[nodiscard]] const char *what() const noexcept override {
                return this->m_shortMessage.c_str();
            }

            [[nodiscard]] std::string format(const std::string &sourceCode, u32 line, u32 column) const {
                return formatImpl(sourceCode, line, column, this->m_prefix, this->m_errorCode, this->m_title, this->m_description, this->m_hint);
            }

        private:
            char m_prefix;
            u32 m_errorCode;
            std::string m_shortMessage;
            std::string m_title, m_description, m_hint;
        };

        RuntimeError(char prefix, u32 errorCode, std::string title) : m_prefix(prefix), m_errorCode(errorCode), m_title(std::move(title)) {

        }

        [[nodiscard]] std::string format(const std::string &description, const std::string &hint = { }, UserData<T> userData = { }) const {
            return Exception(this->m_prefix, this->m_errorCode, this->m_title, description, hint, userData).what();
        }

        [[noreturn]] void throwError(const std::string &description, const std::string &hint = { }, UserData<T> userData = { }) const {
            throw Exception(this->m_prefix, this->m_errorCode, this->m_title, description, hint, userData);
        }

    private:
        char m_prefix;
        u32 m_errorCode;
        std::string m_title;
    };

    std::string formatImpl(
            Location location,
            const std::string& message,
            const std::string& description,
            const std::vector<Location>& trace);

    class CompileError {

    public:
        CompileError(std::string message, Location location) : m_message(std::move(message)), m_location(location) { }
        CompileError(std::string message, std::string description, Location location) : m_message(std::move(message)), m_description(std::move(description)),
                                                                                             m_location(location) { }

        [[nodiscard]] const std::string &getMessage() const { return this->m_message; }
        [[nodiscard]] const std::string &getDescription() const { return this->m_description; }
        [[nodiscard]] const Location &getLocation() const { return this->m_location; }
        [[nodiscard]] Location &getLocation() { return this->m_location; }
        [[nodiscard]] const std::vector<Location> &getTrace() const { return this->m_trace; }
        [[nodiscard]] std::vector<Location> &getTrace() { return this->m_trace; }

        [[nodiscard]] std::string format() const {
            return formatImpl(this->getLocation(), this->getMessage(), this->getDescription(), getTrace());
        }

    private:
        std::string m_message;
        std::string m_description;
        Location m_location;
        std::vector<Location> m_trace;
    };

    class ErrorCollector {
    protected:

        virtual ~ErrorCollector() = default;

        virtual Location location() = 0;

        template <typename... Args>
        inline void error(const fmt::format_string<Args...>& fmt, Args&&... args) {
            this->m_errors.emplace_back(fmt::format(fmt, std::forward<Args>(args)...), location());
        }

        inline void error(const std::string& message) {
            this->m_errors.emplace_back(message, location());
        }

        inline void error_desc(const std::string& message, const std::string& description) {
            this->m_errors.emplace_back(message, description, location());
        }

        template<typename... Args>
        inline void error_desc(const std::string& message, const std::string& description, Args&&... args) {
            this->m_errors.emplace_back(fmt::format(message, std::forward<Args>(args)...), description, location());
        }

        inline void error(CompileError& error) {
            error.getTrace().push_back(location());
            this->m_errors.push_back(std::move(error));
        }

        inline void error_at(const Location& location, const std::string& message) {
            this->m_errors.emplace_back(message, location);
        }

        inline void error_at_desc(const Location& location, const std::string& message, const std::string& description) {
            this->m_errors.emplace_back(message, description, location);
        }

        template<typename... Args>
        inline void error_at(const Location& location, const fmt::format_string<Args...>& message, Args&&... args) {
            this->m_errors.emplace_back(fmt::format(message, std::forward<Args>(args)...), location);
        }

        [[nodiscard]] bool hasErrors() const {
            return !this->m_errors.empty();
        }

        [[nodiscard]] const std::vector<CompileError>& getErrors() const {
            return this->m_errors;
        }

        [[nodiscard]] std::vector<CompileError> collectErrors() {
            if(this->m_errors.empty()) return { };
            auto errors = std::move(this->m_errors);
            this->m_errors.clear();
            return errors;
        }

        void clear() {
            this->m_errors.clear();
        }
    private:
        std::vector<CompileError> m_errors;
    };

}

namespace pl::core {

    template<typename T>
    using CompileResult = hlp::Result<T, err::CompileError>;

}