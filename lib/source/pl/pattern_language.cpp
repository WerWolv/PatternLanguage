#include <pl/pattern_language.hpp>

#include <pl/core/preprocessor.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/lib/std/libstd.hpp>

#include <pl/helpers/fs.hpp>
#include <pl/helpers/file.hpp>

namespace pl {

    PatternLanguage::PatternLanguage(bool addLibStd) {
        this->m_internals.preprocessor  = new core::Preprocessor();
        this->m_internals.lexer         = new core::Lexer();
        this->m_internals.parser        = new core::Parser();
        this->m_internals.validator     = new core::Validator();
        this->m_internals.evaluator     = new core::Evaluator();

        if (addLibStd)
            lib::libstd::registerFunctions(*this);
    }

    PatternLanguage::~PatternLanguage() {
        this->m_flattenedPatterns.clear();
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
        this->m_flattenedPatterns = other.m_flattenedPatterns;
        this->m_running = other.m_running;

        other.m_internals = { };
    }

    std::optional<std::vector<std::shared_ptr<core::ast::ASTNode>>> PatternLanguage::parseString(const std::string &code) {
        auto preprocessedCode = this->m_internals.preprocessor->preprocess(*this, code);
        if (!preprocessedCode.has_value()) {
            this->m_currError = this->m_internals.preprocessor->getError();
            return std::nullopt;
        }

        auto tokens = this->m_internals.lexer->lex(code, preprocessedCode.value());
        if (!tokens.has_value()) {
            this->m_currError = this->m_internals.lexer->getError();
            return std::nullopt;
        }

        auto ast = this->m_internals.parser->parse(code, tokens.value());
        if (!ast.has_value()) {
            this->m_currError = this->m_internals.parser->getError();
            return std::nullopt;
        }

        if (!this->m_internals.validator->validate(code, *ast)) {
            this->m_currError = this->m_internals.validator->getError();

            return std::nullopt;
        }

        return ast;
    }

    bool PatternLanguage::executeString(std::string code, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
        code = hlp::replaceAll(code, "\r\n", "\n");
        code = hlp::replaceAll(code, "\t", "    ");

        this->m_running = true;
        PL_ON_SCOPE_EXIT { this->m_running = false; };

        PL_ON_SCOPE_EXIT {
            if (this->m_currError.has_value()) {
                const auto &error = this->m_currError.value();

                this->m_internals.evaluator->getConsole().log(core::LogConsole::Level::Error, error.message);
            }
        };

        this->reset();
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


        if (!this->m_internals.evaluator->evaluate(code, this->m_currAST)) {
            this->m_currError = this->m_internals.evaluator->getConsole().getLastHardError();
            return false;
        }

        if (auto mainResult = this->m_internals.evaluator->getMainResult(); checkResult && mainResult.has_value()) {
            auto returnCode = core::Token::literalToSigned(*mainResult);

            if (returnCode != 0) {
                this->m_currError = core::err::PatternLanguageError(fmt::format("non-success value returned from main: {}", returnCode), 0, 1);

                return false;
            }
        }

        this->m_flattenedPatterns.clear();

        return true;
    }

    bool PatternLanguage::executeFile(const std::fs::path &path, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables) {
        hlp::fs::File file(path, hlp::fs::File::Mode::Read);

        return this->executeString(file.readString(), envVars, inVariables, true);
    }

    std::pair<bool, std::optional<core::Token::Literal>> PatternLanguage::executeFunction(const std::string &code) {

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

    const std::vector<std::shared_ptr<core::ast::ASTNode>> &PatternLanguage::getCurrentAST() const {
        return this->m_currAST;
    }

    [[nodiscard]] std::map<std::string, core::Token::Literal> PatternLanguage::getOutVariables() const {
        return this->m_internals.evaluator->getOutVariables();
    }


    const std::vector<std::pair<core::LogConsole::Level, std::string>> &PatternLanguage::getConsoleLog() const {
        return this->m_internals.evaluator->getConsole().getLog();
    }

    const std::optional<core::err::PatternLanguageError> &PatternLanguage::getError() const {
        return this->m_currError;
    }

    u32 PatternLanguage::getCreatedPatternCount() const {
        return this->m_internals.evaluator->getPatternCount();
    }

    u32 PatternLanguage::getMaximumPatternCount() const {
        return this->m_internals.evaluator->getPatternLimit();
    }

    [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>> &PatternLanguage::getPatterns() const {
        return this->m_internals.evaluator->getPatterns();
    }


    void PatternLanguage::reset() {
        this->m_flattenedPatterns.clear();

        this->m_currAST.clear();

        this->m_currError.reset();
        this->m_internals.evaluator->getConsole().clear();
        this->m_internals.evaluator->setDefaultEndian(std::endian::native);
        this->m_internals.validator->setRecursionDepth(32);
        this->m_internals.evaluator->setEvaluationDepth(32);
        this->m_internals.evaluator->setArrayLimit(0x10000);
        this->m_internals.evaluator->setPatternLimit(0x20000);
        this->m_internals.evaluator->setLoopLimit(0x1000);
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

        for (const auto &pattern : this->getPatterns()) {
            auto children = pattern->getChildren();

            for (const auto &[address, child]: children) {
                intervals.emplace_back(address, address + child->getSize() - 1, child);
            }
        }

        this->m_flattenedPatterns = std::move(intervals);
    }

    std::vector<ptrn::Pattern *> PatternLanguage::getPatterns(u64 address) const {
        if (this->m_flattenedPatterns.empty())
            return { };

        auto intervals = this->m_flattenedPatterns.findOverlapping(address, address);

        std::vector<ptrn::Pattern*> results;
        std::transform(intervals.begin(), intervals.end(), std::back_inserter(results), [](const auto &interval) {
            ptrn::Pattern* value = interval.value;

            value->setOffset(interval.start);
            value->clearFormatCache();

            return value;
        });

        return results;
    }

}