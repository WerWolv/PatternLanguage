#include "pl/pattern_language.hpp"

#include "pl/preprocessor.hpp"
#include "pl/lexer.hpp"
#include "pl/parser.hpp"
#include "pl/validator.hpp"
#include "pl/evaluator.hpp"
#include "pl/libstd.hpp"

#include "helpers/fs.hpp"
#include "helpers/file.hpp"

namespace pl {

    class Pattern;

    PatternLanguage::PatternLanguage(bool addLibStd) {
        this->m_internals.preprocessor  = new Preprocessor();
        this->m_internals.lexer         = new Lexer();
        this->m_internals.parser        = new Parser();
        this->m_internals.validator     = new Validator();
        this->m_internals.evaluator     = new Evaluator();

        if (addLibStd)
            libstd::registerFunctions(*this);
    }

    PatternLanguage::~PatternLanguage() {
        this->m_flattenedPatterns.clear();
        this->m_patterns.clear();
        this->m_currAST.clear();

        delete this->m_internals.preprocessor;
        delete this->m_internals.lexer;
        delete this->m_internals.parser;
        delete this->m_internals.validator;
        delete this->m_internals.evaluator;
    }

    PatternLanguage::PatternLanguage(PatternLanguage &&other) noexcept {
        this->m_internals = other.m_internals;
        this->m_currError = std::move(other.m_currError);
        this->m_currAST = std::move(other.m_currAST);
        this->m_patterns = std::move(other.m_patterns);
        this->m_flattenedPatterns = other.m_flattenedPatterns;
        this->m_running = other.m_running;

        other.m_internals = { nullptr };
    }

    std::optional<std::vector<std::shared_ptr<ASTNode>>> PatternLanguage::parseString(const std::string &code) {
        auto preprocessedCode = this->m_internals.preprocessor->preprocess(*this, code);
        if (!preprocessedCode.has_value()) {
            this->m_currError = this->m_internals.preprocessor->getError();
            return std::nullopt;
        }

        auto tokens = this->m_internals.lexer->lex(preprocessedCode.value());
        if (!tokens.has_value()) {
            this->m_currError = this->m_internals.lexer->getError();
            return std::nullopt;
        }

        auto ast = this->m_internals.parser->parse(tokens.value());
        if (!ast.has_value()) {
            this->m_currError = this->m_internals.parser->getError();
            return std::nullopt;
        }

        if (!this->m_internals.validator->validate(*ast)) {
            this->m_currError = this->m_internals.validator->getError();

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
                    this->m_internals.evaluator->getConsole().log(LogConsole::Level::Error, fmt::format("{}: {}", error.getLineNumber(), error.what()));
                else
                    this->m_internals.evaluator->getConsole().log(LogConsole::Level::Error, error.what());
            }
        };

        this->m_currError.reset();
        this->m_internals.evaluator->getConsole().clear();
        this->m_internals.evaluator->setDefaultEndian(std::endian::native);
        this->m_internals.evaluator->setEvaluationDepth(32);
        this->m_internals.evaluator->setArrayLimit(0x1000);
        this->m_internals.evaluator->setPatternLimit(0x2000);
        this->m_internals.evaluator->setLoopLimit(0x1000);
        this->m_internals.evaluator->setInVariables(inVariables);

        for (const auto &[name, value] : envVars)
            this->m_internals.evaluator->setEnvVariable(name, value);

        this->m_currAST.clear();

        {
            auto ast = this->parseString(code);
            if (!ast)
                return false;

            this->m_currAST = std::move(ast.value());
        }


        auto patterns = this->m_internals.evaluator->evaluate(this->m_currAST);
        if (!patterns.has_value()) {
            this->m_currError = this->m_internals.evaluator->getConsole().getLastHardError();
            return false;
        }

        if (auto mainResult = this->m_internals.evaluator->getMainResult(); checkResult && mainResult.has_value()) {
            auto returnCode = Token::literalToSigned(*mainResult);

            if (returnCode != 0) {
                this->m_currError = PatternLanguageError(0, fmt::format("non-success value returned from main: {}", returnCode));

                return false;
            }
        }

        this->m_patterns = std::move(patterns.value());
        this->m_flattenedPatterns.clear();

        return true;
    }

    bool PatternLanguage::executeFile(const std::fs::path &path, const std::map<std::string, Token::Literal> &envVars, const std::map<std::string, Token::Literal> &inVariables) {
        fs::File file(path, fs::File::Mode::Read);

        return this->executeString(file.readString(), envVars, inVariables, true);
    }

    std::pair<bool, std::optional<Token::Literal>> PatternLanguage::executeFunction(const std::string &code) {

        auto functionContent = fmt::format("fn main() {{ {0} }};", code);

        auto success = this->executeString(functionContent, {}, {}, false);
        auto result  = this->m_internals.evaluator->getMainResult();

        return { success, std::move(result) };
    }

    void PatternLanguage::abort() const {
        this->m_internals.evaluator->abort();
    }

    void PatternLanguage::setIncludePaths(std::vector<std::fs::path> paths) const {
        this->m_internals.preprocessor->setIncludePaths(std::move(paths));
    }

    void PatternLanguage::addPragma(const std::string &name, const api::PragmaHandler &callback) const {
        this->m_internals.preprocessor->addPragmaHandler(name, callback);
    }

    void PatternLanguage::removePragma(const std::string &name) const {
        this->m_internals.preprocessor->removePragmaHandler(name);
    }

    void PatternLanguage::setDataSource(std::function<void(u64, u8*, size_t)> readFunction, u64 baseAddress, u64 size) const {
        this->m_internals.evaluator->setDataSource(std::move(readFunction), baseAddress, size);
    }

    void PatternLanguage::setDataBaseAddress(u64 baseAddress) const {
        this->m_internals.evaluator->setDataBaseAddress(baseAddress);
    }

    void PatternLanguage::setDataSize(u64 size) const {
        this->m_internals.evaluator->setDataSize(size);
    }

    void PatternLanguage::setDangerousFunctionCallHandler(std::function<bool()> callback) const {
        this->m_internals.evaluator->setDangerousFunctionCallHandler(std::move(callback));
    }

    const std::vector<std::shared_ptr<ASTNode>> &PatternLanguage::getCurrentAST() const {
        return this->m_currAST;
    }

    [[nodiscard]] std::map<std::string, Token::Literal> PatternLanguage::getOutVariables() const {
        return this->m_internals.evaluator->getOutVariables();
    }


    const std::vector<std::pair<LogConsole::Level, std::string>> &PatternLanguage::getConsoleLog() const {
        return this->m_internals.evaluator->getConsole().getLog();
    }

    const std::optional<PatternLanguageError> &PatternLanguage::getError() const {
        return this->m_currError;
    }

    u32 PatternLanguage::getCreatedPatternCount() const {
        return this->m_internals.evaluator->getPatternCount();
    }

    u32 PatternLanguage::getMaximumPatternCount() const {
        return this->m_internals.evaluator->getPatternLimit();
    }


    void PatternLanguage::reset() {
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();

        this->m_currAST.clear();
    }


    static std::string getFunctionName(const api::Namespace &ns, const std::string &name) {
        std::string functionName;
        for (auto &scope : ns)
            functionName += scope + "::";

        functionName += name;

        return functionName;
    }

    void PatternLanguage::addFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) const {
        this->m_internals.evaluator->addBuiltinFunction(getFunctionName(ns, name), parameterCount, { }, func, false);
    }

    void PatternLanguage::addDangerousFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) const {
        this->m_internals.evaluator->addBuiltinFunction(getFunctionName(ns, name), parameterCount, { }, func, true);
    }

    void PatternLanguage::flattenPatterns() {
        using Interval = decltype(this->m_flattenedPatterns)::interval;
        std::vector<Interval> intervals;

        for (const auto &pattern : this->m_patterns) {
            auto children = pattern->getChildren();

            for (const auto &[address, child]: children) {
                intervals.emplace_back(address, address + child->getSize() - 1, child);
            }
        }

        this->m_flattenedPatterns = std::move(intervals);
    }

    std::vector<Pattern *> PatternLanguage::getPatterns(u64 address) const {
        if (this->m_flattenedPatterns.empty())
            return { };

        auto intervals = this->m_flattenedPatterns.findOverlapping(address, address);

        std::vector<Pattern*> results;
        std::transform(intervals.begin(), intervals.end(), std::back_inserter(results), [](const auto &interval) {
            return interval.value;
        });

        return results;
    }

}