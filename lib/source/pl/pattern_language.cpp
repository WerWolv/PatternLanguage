#include "pl/pattern_language.hpp"

#include "pl/preprocessor.hpp"
#include "pl/lexer.hpp"
#include "pl/parser.hpp"
#include "pl/validator.hpp"
#include "pl/evaluator.hpp"

#include "helpers/fs.hpp"
#include "helpers/file.hpp"

namespace pl {

    class Pattern;

    PatternLanguage::PatternLanguage() {
        this->m_preprocessor = new Preprocessor();
        this->m_lexer        = new Lexer();
        this->m_parser       = new Parser();
        this->m_validator    = new Validator();
        this->m_evaluator    = new Evaluator();

        this->m_preprocessor->addDefaultPragmaHandlers();
    }

    PatternLanguage::~PatternLanguage() {
        delete this->m_preprocessor;
        delete this->m_lexer;
        delete this->m_parser;
        delete this->m_validator;
    }

    std::optional<std::vector<std::shared_ptr<ASTNode>>> PatternLanguage::parseString(const std::string &code) {
        auto preprocessedCode = this->m_preprocessor->preprocess(code);
        if (!preprocessedCode.has_value()) {
            this->m_currError = this->m_preprocessor->getError();
            return std::nullopt;
        }

        auto tokens = this->m_lexer->lex(preprocessedCode.value());
        if (!tokens.has_value()) {
            this->m_currError = this->m_lexer->getError();
            return std::nullopt;
        }

        auto ast = this->m_parser->parse(tokens.value());
        if (!ast.has_value()) {
            this->m_currError = this->m_parser->getError();
            return std::nullopt;
        }

        if (!this->m_validator->validate(*ast)) {
            this->m_currError = this->m_validator->getError();

            return std::nullopt;
        }

        return ast;
    }

    bool PatternLanguage::executeString(const std::string &code, const std::map<std::string, Token::Literal> &envVars, const std::map<std::string, Token::Literal> &inVariables, bool checkResult) {
        this->m_running = true;
        PL_ON_SCOPE_EXIT { this->m_running = false; };

        PL_ON_SCOPE_EXIT {
            if (this->m_currError.has_value()) {
                const auto &error = this->m_currError.value();

                if (error.getLineNumber() > 0)
                    this->m_evaluator->getConsole().log(LogConsole::Level::Error, fmt::format("{}: {}", error.getLineNumber(), error.what()));
                else
                    this->m_evaluator->getConsole().log(LogConsole::Level::Error, error.what());
            }
        };

        this->m_currError.reset();
        this->m_evaluator->getConsole().clear();
        this->m_evaluator->setDefaultEndian(std::endian::native);
        this->m_evaluator->setEvaluationDepth(32);
        this->m_evaluator->setArrayLimit(0x1000);
        this->m_evaluator->setPatternLimit(0x2000);
        this->m_evaluator->setLoopLimit(0x1000);
        this->m_evaluator->setInVariables(inVariables);

        for (const auto &[name, value] : envVars)
            this->m_evaluator->setEnvVariable(name, value);

        this->m_currAST.clear();

        {
            auto ast = this->parseString(code);
            if (!ast)
                return false;

            this->m_currAST = std::move(ast.value());
        }


        auto patterns = this->m_evaluator->evaluate(this->m_currAST);
        if (!patterns.has_value()) {
            this->m_currError = this->m_evaluator->getConsole().getLastHardError();
            return false;
        }

        if (auto mainResult = this->m_evaluator->getMainResult(); checkResult && mainResult.has_value()) {
            auto returnCode = Token::literalToSigned(*mainResult);

            if (returnCode != 0) {
                this->m_currError = PatternLanguageError(0, fmt::format("non-success value returned from main: {}", returnCode));

                return false;
            }
        }

        this->m_patterns = std::move(patterns.value());

        return true;
    }

    bool PatternLanguage::executeFile(const std::fs::path &path, const std::map<std::string, Token::Literal> &envVars, const std::map<std::string, Token::Literal> &inVariables) {
        fs::File file(path, fs::File::Mode::Read);

        return this->executeString(file.readString(), envVars, inVariables, true);
    }

    std::pair<bool, std::optional<Token::Literal>> PatternLanguage::executeFunction(const std::string &code) {

        auto functionContent = fmt::format("fn main() {{ {0} }};", code);

        auto success = this->executeString(functionContent, {}, {}, false);
        auto result  = this->m_evaluator->getMainResult();

        return { success, std::move(result) };
    }

    void PatternLanguage::abort() {
        this->m_evaluator->abort();
    }

    void PatternLanguage::setIncludePaths(std::vector<std::fs::path> paths) {
        this->m_preprocessor->setIncludePaths(std::move(paths));
    }

    void PatternLanguage::setDataSource(std::function<void(u64, u8*, size_t)> readFunction, u64 baseAddress, u64 size) {
        this->m_evaluator->setDataSource(std::move(readFunction), baseAddress, size);
    }

    void PatternLanguage::setDataBaseAddress(u64 baseAddress) {
        this->m_evaluator->setDataBaseAddress(baseAddress);
    }

    void PatternLanguage::setDataSize(u64 size) {
        this->m_evaluator->setDataSize(size);
    }

    void PatternLanguage::setDangerousFunctionCallHandler(std::function<bool()> callback) {
        this->m_evaluator->setDangerousFunctionCallHandler(std::move(callback));
    }

    const std::vector<std::shared_ptr<ASTNode>> &PatternLanguage::getCurrentAST() const {
        return this->m_currAST;
    }

    [[nodiscard]] std::map<std::string, Token::Literal> PatternLanguage::getOutVariables() const {
        return this->m_evaluator->getOutVariables();
    }


    const std::vector<std::pair<LogConsole::Level, std::string>> &PatternLanguage::getConsoleLog() {
        return this->m_evaluator->getConsole().getLog();
    }

    const std::optional<PatternLanguageError> &PatternLanguage::getError() {
        return this->m_currError;
    }

    u32 PatternLanguage::getCreatedPatternCount() {
        return this->m_evaluator->getPatternCount();
    }

    u32 PatternLanguage::getMaximumPatternCount() {
        return this->m_evaluator->getPatternLimit();
    }


    void PatternLanguage::reset() {
        this->m_patterns.clear();

        this->m_currAST.clear();
    }


    static std::string getFunctionName(const api::Namespace &ns, const std::string &name) {
        std::string functionName;
        for (auto &scope : ns)
            functionName += scope + "::";

        functionName += name;

        return functionName;
    }

    void PatternLanguage::addFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) {
        this->m_evaluator->addBuiltinFunction(getFunctionName(ns, name), parameterCount, { }, func, false);
    }

    void PatternLanguage::addDangerousFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) {
        this->m_evaluator->addBuiltinFunction(getFunctionName(ns, name), parameterCount, { }, func, true);
    }

}