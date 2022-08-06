#pragma once

#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#include <helpers/utils.hpp>

#include <fmt/format.h>

namespace pl::err {

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

    struct PatternLanguageError {
        PatternLanguageError(std::string message, u32 line, u32 column) : message(std::move(message)), line(line), column(column) { }

        std::string message;
        u32 line, column;
    };

    template<>
    class UserData<void> { };

    template<typename T = void>
    class Exception : public std::exception, public UserData<T> {
    public:
        Exception(char prefix, u32 errorCode, std::string title, std::string description, std::string hint, UserData<T> userData = {}) :
                UserData<T>(userData), m_prefix(prefix), m_errorCode(errorCode), m_title(std::move(title)), m_description(std::move(description)), m_hint(std::move(hint)) {
            this->m_shortMessage = fmt::format("error[{}{:04}]: {} -- {}", this->m_prefix, this->m_errorCode, this->m_title, this->m_description).c_str();
        }

        [[nodiscard]] const char *what() const noexcept override {
            return this->m_shortMessage.c_str();
        }

        [[nodiscard]] std::string format(const std::string &sourceCode, u32 line, u32 column) const {
            std::string errorMessage;

            errorMessage += fmt::format("error[{}{:04}]: {}\n", this->m_prefix, this->m_errorCode, this->m_title);
            errorMessage += fmt::format("  --> <Source Code>:{}:{}\n", line, column);

            auto lines = splitString(sourceCode, "\n");

            if ((line - 1) < lines.size()) {
                const auto &errorLine = lines[line - 1];
                const auto lineNumberPrefix = fmt::format("{} | ", line);
                errorMessage += fmt::format("{}{}\n", lineNumberPrefix, errorLine);

                {
                    const auto descriptionSpacing = std::string(lineNumberPrefix.length() + column, ' ');
                    errorMessage += descriptionSpacing + "^\n";
                    errorMessage += descriptionSpacing + this->m_description + "\n\n";
                }
            }

            if (!this->m_hint.empty()) {
                errorMessage += fmt::format("hint: {}", this->m_hint);
            }

            return errorMessage;
        }

    private:
        std::string m_shortMessage;
        std::string m_title, m_description, m_hint;
        char m_prefix;
        u32 m_errorCode;

        static std::vector<std::string> splitString(const std::string &string, const std::string &delimiter) {
            size_t start = 0, end = 0;
            std::string token;
            std::vector<std::string> res;

            while ((end = string.find(delimiter, start)) != std::string::npos) {
                size_t size = end - start;
                if (start + size > string.length())
                    break;
                if (start > end)
                    break;

                token = string.substr(start, end - start);
                start = end + delimiter.length();
                res.push_back(token);
            }

            res.emplace_back(string.substr(start));
            return res;
        }
    };

    template<typename T = void>
    class Error {
    public:
        Error(char prefix, u32 errorCode, std::string title) : m_prefix(prefix), m_errorCode(errorCode), m_title(std::move(title)) {

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