#pragma once

#include <functional>
#include <optional>
#include <string>

#include <pl/helpers/types.hpp>
#include <pl/core/errors/evaluator_errors.hpp>

namespace pl::core {

    namespace ast { class ASTNode; }

    class LogConsole {
    public:
        enum class Level : u8 {
            Debug       = 0,
            Info        = 1,
            Warning     = 2,
            Error       = 3
        };

        using Callback = std::function<void(Level level, const std::string&)>;

        void log(Level level, const std::string &message) const {
            if (u8(level) >= u8(this->m_logLevel) && this->m_logCallback)
                this->m_logCallback(level, message);
        }

        void clear() {
            this->m_lastHardError.reset();
        }

        void setHardError(const err::PatternLanguageError &error) { this->m_lastHardError = error; }

        [[nodiscard]] const std::optional<err::PatternLanguageError> &getLastHardError() { return this->m_lastHardError; };

        void setLogLevel(Level level) {
            this->m_logLevel = level;
        }

        void setLogCallback(const Callback &callback) {
            this->m_logCallback = callback;
        }

    private:
        Level m_logLevel = Level::Info;

        Callback m_logCallback;
        std::optional<err::PatternLanguageError> m_lastHardError;
    };

}