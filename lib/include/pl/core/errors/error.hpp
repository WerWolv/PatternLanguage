#pragma once

#include <pl/helpers/types.hpp>

#include <string>
#include <stdexcept>
#include <vector>

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
    class Error {
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

        Error(char prefix, u32 errorCode, std::string title) : m_prefix(prefix), m_errorCode(errorCode), m_title(std::move(title)) {

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

}