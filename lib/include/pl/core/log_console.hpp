#pragma once


#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>

#include <pl/helpers/types.hpp>
#include <pl/core/errors/evaluator_errors.hpp>

namespace pl::core {

    namespace ast { class ASTNode; }

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

        void clear() {
            this->m_consoleLog.clear();
            this->m_lastHardError.reset();
        }

        void setHardError(const err::PatternLanguageError &error) { this->m_lastHardError = error; }

        [[nodiscard]] const std::optional<err::PatternLanguageError> &getLastHardError() { return this->m_lastHardError; };

    private:
        std::vector<std::pair<Level, std::string>> m_consoleLog;

        std::optional<err::PatternLanguageError> m_lastHardError;
        ast::ASTNode *m_lastHardErrorNode = nullptr;
    };

}