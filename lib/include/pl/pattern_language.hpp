#pragma once

#include <atomic>
#include <bit>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

#include <IntervalTree.h>

#include <pl/api.hpp>

#include <pl/core/log_console.hpp>
#include <pl/core/token.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/helpers/types.hpp>

#include <wolv/io/fs.hpp>

namespace hex::prv {
    class Provider;
}

namespace pl {

    namespace core {
        class Preprocessor;
        class Lexer;
        class Parser;
        class Validator;
        class Evaluator;

        namespace ast { class ASTNode; }
    }

    namespace ptrn {
        class Pattern;
        class Iteratable;
    }

    class PatternLanguage {
    public:
        explicit PatternLanguage(bool addLibStd = true);
        ~PatternLanguage();

        PatternLanguage(const PatternLanguage&) = delete;
        PatternLanguage(PatternLanguage &&other) noexcept;

        struct Internals {
            core::Preprocessor    *preprocessor;
            core::Lexer           *lexer;
            core::Parser          *parser;
            core::Validator       *validator;
            core::Evaluator       *evaluator;
        };

        [[nodiscard]] std::optional<std::vector<std::shared_ptr<core::ast::ASTNode>>> parseString(const std::string &code);
        [[nodiscard]] bool executeString(std::string string, const std::map<std::string, core::Token::Literal> &envVars = {}, const std::map<std::string, core::Token::Literal> &inVariables = {}, bool checkResult = true);
        [[nodiscard]] bool executeFile(const std::filesystem::path &path, const std::map<std::string, core::Token::Literal> &envVars = {}, const std::map<std::string, core::Token::Literal> &inVariables = {}, bool checkResult = true);
        [[nodiscard]] std::pair<bool, std::optional<core::Token::Literal>> executeFunction(const std::string &code);
        [[nodiscard]] const std::vector<std::shared_ptr<core::ast::ASTNode>> &getCurrentAST() const;


        void abort();

        void setDataSource(u64 baseAddress, u64 size, std::function<void(u64, u8*, size_t)> readFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writerFunction = std::nullopt) const;
        void setDataBaseAddress(u64 baseAddress) const;
        void setDataSize(u64 size) const;
        void setDefaultEndian(std::endian endian);
        void setStartAddress(u64 address);


        void addPragma(const std::string &name, const api::PragmaHandler &callback) const;
        void removePragma(const std::string &name) const;
        void addDefine(const std::string &name, const std::string &value = "") const;
        void setIncludePaths(std::vector<std::fs::path> paths) const;
        void setDangerousFunctionCallHandler(std::function<bool()> callback) const;

        [[nodiscard]] const std::vector<std::pair<core::LogConsole::Level, std::string>> &getConsoleLog() const;
        [[nodiscard]] const std::optional<core::err::PatternLanguageError> &getError() const;
        [[nodiscard]] std::map<std::string, core::Token::Literal> getOutVariables() const;

        [[nodiscard]] u32 getCreatedPatternCount() const;
        [[nodiscard]] u32 getMaximumPatternCount() const;

        [[nodiscard]] const std::vector<u8>& getSection(u64 id);
        [[nodiscard]] const std::map<u64, api::Section>& getSections() const;

        [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>> &getAllPatterns(u64 section = 0x00) const;
        [[nodiscard]] std::vector<ptrn::Pattern *> getPatternsAtAddress(u64 address, u64 section = 0x00) const;

        void reset();
        [[nodiscard]] bool isRunning() const { return this->m_running; }
        [[nodiscard]] const std::chrono::duration<double> & getLastRunningTime() const { return this->m_runningTime; }

        void addFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) const;
        void addDangerousFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) const;

        [[nodiscard]] const Internals& getInternals() const {
            return this->m_internals;
        }

        void addCleanupCallback(const std::function<void(PatternLanguage&)> &callback) {
            this->m_cleanupCallbacks.push_back(callback);
        }

    private:
        void flattenPatterns();

    private:

        Internals m_internals;
        std::optional<core::err::PatternLanguageError> m_currError;

        std::vector<std::shared_ptr<core::ast::ASTNode>> m_currAST;
        std::map<u64, std::vector<std::shared_ptr<ptrn::Pattern>>> m_patterns;
        std::map<u64, interval_tree::IntervalTree<u64, ptrn::Pattern*>> m_flattenedPatterns;
        std::vector<std::function<void(PatternLanguage&)>> m_cleanupCallbacks;

        bool m_running = false;
        std::atomic<bool> m_aborted = false;

        std::optional<u64> m_startAddress;
        std::endian m_defaultEndian = std::endian::little;
        std::chrono::duration<double> m_runningTime = std::chrono::duration<double>::zero();
    };

}