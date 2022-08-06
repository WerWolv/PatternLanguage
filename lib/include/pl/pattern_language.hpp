#pragma once


#include <bit>
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

#include "helpers/fs.hpp"

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
    }

    class PatternLanguage {
    public:
        PatternLanguage(bool addLibStd = true);
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
        [[nodiscard]] bool executeString(const std::string &string, const std::map<std::string, core::Token::Literal> &envVars = {}, const std::map<std::string, core::Token::Literal> &inVariables = {}, bool checkResult = true);
        [[nodiscard]] bool executeFile(const std::filesystem::path &path, const std::map<std::string, core::Token::Literal> &envVars = {}, const std::map<std::string, core::Token::Literal> &inVariables = {});
        [[nodiscard]] std::pair<bool, std::optional<core::Token::Literal>> executeFunction(const std::string &code);
        [[nodiscard]] const std::vector<std::shared_ptr<core::ast::ASTNode>> &getCurrentAST() const;

        void abort() const;

        void setDataSource(std::function<void(u64, u8*, size_t)> readFunction, u64 baseAddress, u64 size) const;
        void setDataBaseAddress(u64 baseAddress) const;
        void setDataSize(u64 size) const;

        void addPragma(const std::string &name, const api::PragmaHandler &callback) const;
        void removePragma(const std::string &name) const;
        void setIncludePaths(std::vector<std::fs::path> paths) const;
        void setDangerousFunctionCallHandler(std::function<bool()> callback) const;

        [[nodiscard]] const std::vector<std::pair<core::LogConsole::Level, std::string>> &getConsoleLog() const;
        [[nodiscard]] const std::optional<core::err::PatternLanguageError> &getError() const;
        [[nodiscard]] std::map<std::string, core::Token::Literal> getOutVariables() const;

        [[nodiscard]] u32 getCreatedPatternCount() const;
        [[nodiscard]] u32 getMaximumPatternCount() const;

        [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>> &getPatterns() const {
            const static std::vector<std::shared_ptr<ptrn::Pattern>> empty;

            if (isRunning()) return empty;
            else return this->m_patterns;
        }

        void flattenPatterns();
        [[nodiscard]] const interval_tree::IntervalTree<u64, ptrn::Pattern*>& getFlattenedPatterns() const {
            return this->m_flattenedPatterns;
        }

        [[nodiscard]] std::vector<ptrn::Pattern *> getPatterns(u64 address) const;

        void reset();
        [[nodiscard]] bool isRunning() const { return this->m_running; }


        void addFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) const;
        void addDangerousFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) const;

        [[nodiscard]] const Internals& getInternals() const {
            return this->m_internals;
        }

    private:
        Internals m_internals;

        std::vector<std::shared_ptr<core::ast::ASTNode>> m_currAST;

        std::optional<core::err::PatternLanguageError> m_currError;

        std::vector<std::shared_ptr<ptrn::Pattern>> m_patterns;

        interval_tree::IntervalTree<u64, ptrn::Pattern*> m_flattenedPatterns;

        bool m_running = false;
    };

}