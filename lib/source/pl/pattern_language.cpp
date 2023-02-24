#include <pl/pattern_language.hpp>

#include <pl/core/preprocessor.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/lib/std/libstd.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>

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
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();
        this->m_currAST.clear();

        delete this->m_internals.preprocessor;
        delete this->m_internals.lexer;
        delete this->m_internals.parser;
        delete this->m_internals.validator;
        delete this->m_internals.evaluator;
    }

    PatternLanguage::PatternLanguage(PatternLanguage &&other) noexcept {
        this->m_internals           = other.m_internals;
        other.m_internals = { };

        this->m_currError           = std::move(other.m_currError);
        this->m_currAST             = std::move(other.m_currAST);

        this->m_patterns            = std::move(other.m_patterns);
        this->m_flattenedPatterns   = other.m_flattenedPatterns;

        this->m_running             = other.m_running;
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

        if (!this->m_internals.validator->validate(code, *ast, true, true)) {
            this->m_currError = this->m_internals.validator->getError();

            return std::nullopt;
        }

        return ast;
    }

    bool PatternLanguage::executeString(std::string code, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
        auto startTime = std::chrono::high_resolution_clock::now();
        ON_SCOPE_EXIT {
            auto endTime = std::chrono::high_resolution_clock::now();
            this->m_runningTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
        };

        code = wolv::util::replaceStrings(code, "\r\n", "\n");
        code = wolv::util::replaceStrings(code, "\t", "    ");

        auto &evaluator = this->m_internals.evaluator;

        this->m_running = true;
        this->m_aborted = false;
        ON_SCOPE_EXIT { this->m_running = false; };

        ON_SCOPE_EXIT {
            if (this->m_currError.has_value()) {
                const auto &error = this->m_currError.value();

                evaluator->getConsole().log(core::LogConsole::Level::Error, error.message);
            }

            for (const auto &cleanupCallback : this->m_cleanupCallbacks)
                cleanupCallback(*this);
        };

        this->reset();
        evaluator->setInVariables(inVariables);

        for (const auto &[name, value] : envVars)
            evaluator->setEnvVariable(name, value);

        this->m_currAST.clear();

        {
            auto ast = this->parseString(code);
            if (!ast)
                return false;

            this->m_currAST = std::move(ast.value());
        }

        evaluator->dataOffset() = this->m_startAddress.value_or(evaluator->getDataBaseAddress());


        if (!evaluator->evaluate(code, this->m_currAST)) {
            this->m_currError = evaluator->getConsole().getLastHardError();
            return false;
        }

        auto returnCode = evaluator->getMainResult().value_or(0).toSigned();
        evaluator->getConsole().log(core::LogConsole::Level::Info, fmt::format("Pattern exited with code: {}", returnCode));

        if (checkResult && returnCode != 0) {
            this->m_currError = core::err::PatternLanguageError(core::err::E0009.format(fmt::format("Pattern exited with non-zero result: {}", returnCode)), 0, 1);

            return false;
        }

        for (const auto &pattern : evaluator->getPatterns())
            this->m_patterns[pattern->getSection()].push_back(pattern);
        this->m_patterns.erase(ptrn::Pattern::HeapSectionId);

        this->flattenPatterns();

        if (this->m_aborted) {
            this->reset();
        }

        return true;
    }

    bool PatternLanguage::executeFile(const std::fs::path &path, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (!file.isValid())
            return false;

        return this->executeString(file.readString(), envVars, inVariables, checkResult);
    }

    std::pair<bool, std::optional<core::Token::Literal>> PatternLanguage::executeFunction(const std::string &code) {

        auto functionContent = fmt::format("fn main() {{ {0} }};", code);

        auto success = this->executeString(functionContent, {}, {}, false);
        auto result  = this->m_internals.evaluator->getMainResult();

        return { success, std::move(result) };
    }

    void PatternLanguage::abort() {
        this->m_internals.evaluator->abort();
        this->m_aborted = true;
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

    void PatternLanguage::addDefine(const std::string &name, const std::string &value) const {
        this->m_internals.preprocessor->addDefine(name, value);
    }

    void PatternLanguage::setDataSource(u64 baseAddress, u64 size, std::function<void(u64, u8*, size_t)> readFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writeFunction) const {
        this->m_internals.evaluator->setDataSource(baseAddress, size, std::move(readFunction), std::move(writeFunction));
    }

    void PatternLanguage::setDataBaseAddress(u64 baseAddress) const {
        this->m_internals.evaluator->setDataBaseAddress(baseAddress);
    }

    void PatternLanguage::setDataSize(u64 size) const {
        this->m_internals.evaluator->setDataSize(size);
    }

    void PatternLanguage::setDefaultEndian(std::endian endian) {
        this->m_defaultEndian = endian;
    }

    void PatternLanguage::setStartAddress(u64 address) {
        this->m_startAddress = address;
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

    const std::vector<u8>& PatternLanguage::getSection(u64 id) {
        static std::vector<u8> empty;
        if (id > this->m_internals.evaluator->getSectionCount())
            return empty;
        else if (id == ptrn::Pattern::MainSectionId)
            return empty;
        else if (id == ptrn::Pattern::HeapSectionId)
            return empty;
        else
            return this->m_internals.evaluator->getSection(id);
    }

    [[nodiscard]] const std::map<u64, api::Section>& PatternLanguage::getSections() const {
        return this->m_internals.evaluator->getSections();
    }

    [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>> &PatternLanguage::getAllPatterns(u64 section) const {
        static const std::vector<std::shared_ptr<pl::ptrn::Pattern>> empty;
        if (this->m_patterns.contains(section))
            return this->m_patterns.at(section);
        else
            return empty;
    }


    void PatternLanguage::reset() {
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();

        this->m_currAST.clear();

        this->m_currError.reset();
        this->m_internals.validator->setRecursionDepth(32);

        this->m_internals.evaluator->getConsole().clear();
        this->m_internals.evaluator->setDefaultEndian(this->m_defaultEndian);
        this->m_internals.evaluator->setBitfieldOrder({});
        this->m_internals.evaluator->setEvaluationDepth(32);
        this->m_internals.evaluator->setArrayLimit(0x10000);
        this->m_internals.evaluator->setPatternLimit(0x20000);
        this->m_internals.evaluator->setLoopLimit(0x1000);
        this->m_internals.evaluator->setDebugMode(false);
    }


    static std::string getFunctionName(const api::Namespace &ns, const std::string &name) {
        std::string functionName;


        for (auto &scope : ns)
            functionName += fmt::format("{}::", scope);

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
        using Interval = interval_tree::Interval<u64, ptrn::Pattern*>;

        for (const auto &[section, patterns] : this->m_patterns) {
            std::vector<Interval> intervals;
            for (const auto &pattern : patterns) {
                auto children = pattern->getChildren();

                intervals.reserve(children.size());
                for (const auto &[address, child]: children) {
                    if (this->m_aborted)
                        return;

                    if (child->getSize() == 0)
                        continue;

                    intervals.emplace_back(address, address + child->getSize() - 1, child);
                }
            }

            this->m_flattenedPatterns[section] = std::move(intervals);
        }
    }

    std::vector<ptrn::Pattern *> PatternLanguage::getPatternsAtAddress(u64 address, u64 section) const {
        if (this->m_flattenedPatterns.empty() || !this->m_flattenedPatterns.contains(section))
            return { };

        auto intervals = this->m_flattenedPatterns.at(section).findOverlapping(address, address);

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