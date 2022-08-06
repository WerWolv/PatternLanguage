#pragma once

#include <helpers/types.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>

#include <pl/error.hpp>
#include <pl/errors/evaluator_errors.hpp>

namespace pl {

    class ASTNode;

    class LogConsole {
    public:
        enum class Level : u8 {
            Debug,
            Info,
            Warning,
            Error
        };

        [[nodiscard]] const auto &getLog() const { return this->m_consoleLog; }

        void log(Level level, const std::string &message) {
            this->m_consoleLog.emplace_back(level, message);
        }

        [[noreturn]] static void abortEvaluation(const std::string &message) {
            abortEvaluation(message, nullptr);
        }

        template<typename T = ASTNode>
        [[noreturn]] static void abortEvaluation(const std::string &message, const std::unique_ptr<T> &node) {
            abortEvaluation(message, node.get());
        }

        [[noreturn]] static void abortEvaluation(const std::string &message, const ASTNode *node);

        void clear() {
            this->m_consoleLog.clear();
            this->m_lastHardError.reset();
        }

        void setHardError(const err::Error::Exception &error) { this->m_lastHardError = error; }

        [[nodiscard]] const std::optional<err::Error::Exception> &getLastHardError() { return this->m_lastHardError; };

    private:
        std::vector<std::pair<Level, std::string>> m_consoleLog;
        std::optional<err::Error::Exception> m_lastHardError;
    };

}