#include <pl/pattern_language.hpp>

#include <pl/core/preprocessor.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/patterns/pattern.hpp>

#include <pl/lib/std/libstd.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

#include <iostream>

namespace pl {

    PatternLanguage::PatternLanguage(const bool addLibStd) {
        this->m_internals = {
            .preprocessor   = std::make_unique<core::Preprocessor>(),
            .lexer          = std::make_unique<core::Lexer>(),
            .parser         = std::make_unique<core::Parser>(),
            .validator      = std::make_unique<core::Validator>(),
            .evaluator      = std::make_unique<core::Evaluator>()
        };

        if (addLibStd)
            lib::libstd::registerFunctions(*this);
    }

    PatternLanguage::~PatternLanguage() {
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();
    }

    PatternLanguage::PatternLanguage(PatternLanguage &&other) noexcept {
        this->m_internals           = std::move(other.m_internals);
        other.m_internals = { };

        this->m_currError           = std::move(other.m_currError);

        this->m_patterns            = std::move(other.m_patterns);
        this->m_flattenedPatterns   = other.m_flattenedPatterns;

        this->m_running.exchange(other.m_running.load());
    }

    std::optional<std::vector<std::shared_ptr<core::ast::ASTNode>>> PatternLanguage::parseString(const std::string &code, const std::string &source) {
        const auto sourceObj = m_resolvers.setSource(code, source);

        auto [preprocessedCode, preprocessorErrors] = this->m_internals.preprocessor->preprocess(this, sourceObj);
        if (!preprocessorErrors.empty()) {
            this->m_compErrors = std::move(preprocessorErrors);
            return std::nullopt;
        }

        sourceObj->content = preprocessedCode.value(); // update source object with preprocessed code

        auto [tokens, lexerErrors] = this->m_internals.lexer->lex(sourceObj);

        if (!lexerErrors.empty()) {
            this->m_compErrors = std::move(lexerErrors);
            return std::nullopt;
        }

        auto [ast, parserErrors] = this->m_internals.parser->parse(code, tokens.value());
        if (!parserErrors.empty()) {
            this->m_compErrors = std::move(parserErrors);
            return std::nullopt;
        }

        if (!this->m_internals.validator->validate(code, *ast, true, true)) {
            return std::nullopt;
        }

        return ast;
    }

    bool PatternLanguage::executeString(std::string code, const std::string& source, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
        const auto startTime = std::chrono::high_resolution_clock::now();
        ON_SCOPE_EXIT {
            const auto endTime = std::chrono::high_resolution_clock::now();
            this->m_runningTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
        };

        code = wolv::util::replaceStrings(code, "\r\n", "\n");
        code = wolv::util::replaceStrings(code, "\t", "    ");

        const auto &evaluator = this->m_internals.evaluator;

        this->m_running = true;
        this->m_aborted = false;
        this->m_runId += 1;
        ON_SCOPE_EXIT { this->m_running = false; };

        ON_SCOPE_EXIT {
            for (const auto &error: this->m_compErrors) {
                evaluator->getConsole().log(core::LogConsole::Level::Error, error.format());
            }

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

        auto ast = this->parseString(code, source);
        if (!ast.has_value())
            return false;

        this->m_currAST = std::move(*ast);

        evaluator->setReadOffset(this->m_startAddress.value_or(evaluator->getDataBaseAddress()));

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
        } else {
            this->m_patternsValid = true;
        }

        return true;
    }

    bool PatternLanguage::executeFile(const std::fs::path &path, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (!file.isValid())
            return false;

        return this->executeString(file.readString(), path.string(), envVars, inVariables, checkResult);
    }

    std::pair<bool, std::optional<core::Token::Literal>> PatternLanguage::executeFunction(const std::string &code) {

        const auto functionContent = fmt::format("fn main() {{ {0} }};", code);

        auto success = this->executeString(functionContent, core::DefaultSource, {}, {}, false);
        auto result  = this->m_internals.evaluator->getMainResult();

        return { success, std::move(result) };
    }

    api::Source *PatternLanguage::addSource(const std::string &code, const std::string &source) {
        return this->m_resolvers.addSource(code, source);
    }

    api::Source *PatternLanguage::setSource(const std::string &code, const std::string &source) {
        return this->m_resolvers.setSource(code, source);
    }

    void PatternLanguage::abort() {
        this->m_internals.evaluator->abort();
        this->m_aborted = true;
    }

    void PatternLanguage::setIncludePaths(const std::vector<std::fs::path>& paths) const {
        this->m_resolvers.setDefaultResolver(core::FileResolver { paths });
        setIncludeResolver(m_resolvers);
    }

    void PatternLanguage::setIncludeResolver(const api::IncludeResolver &resolver) const {
        this->m_internals.preprocessor->setIncludeResolver(resolver);
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

    [[nodiscard]] std::map<std::string, core::Token::Literal> PatternLanguage::getOutVariables() const {
        return this->m_internals.evaluator->getOutVariables();
    }


    void PatternLanguage::setLogCallback(const core::LogConsole::Callback &callback) const {
        this->m_internals.evaluator->getConsole().setLogCallback(callback);
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

    const std::vector<u8>& PatternLanguage::getSection(u64 id) const {
        static std::vector<u8> empty;
        if (id > this->m_internals.evaluator->getSectionCount() || id == ptrn::Pattern::MainSectionId || id == ptrn::Pattern::HeapSectionId)
            return empty;
        else
            return this->m_internals.evaluator->getSection(id);
    }

    [[nodiscard]] const std::map<u64, api::Section>& PatternLanguage::getSections() const {
        return this->m_internals.evaluator->getSections();
    }

    [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>> &PatternLanguage::getPatterns(u64 section) const {
        static const std::vector<std::shared_ptr<pl::ptrn::Pattern>> empty;
        if (this->m_patterns.contains(section))
            return this->m_patterns.at(section);
        else
            return empty;
    }


    void PatternLanguage::reset() {
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();

        this->m_currError.reset();
        this->m_compErrors.clear();
        this->m_internals.validator->setRecursionDepth(32);

        this->m_internals.evaluator->getConsole().clear();
        this->m_internals.evaluator->setDefaultEndian(this->m_defaultEndian);
        this->m_internals.evaluator->setEvaluationDepth(32);
        this->m_internals.evaluator->setArrayLimit(0x10000);
        this->m_internals.evaluator->setPatternLimit(0x20000);
        this->m_internals.evaluator->setLoopLimit(0x1000);
        this->m_internals.evaluator->setDebugMode(false);
        this->m_patternsValid = false;
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

        for (const auto &[section, patterns] : this->m_patterns) {
            auto &sectionTree = this->m_flattenedPatterns[section];
            for (const auto &pattern : patterns) {
                auto children = pattern->getChildren();

                for (const auto &[address, child]: children) {
                    if (this->m_aborted)
                        return;

                    if (child->getSize() == 0)
                        continue;

                    sectionTree.insert({ address, address + child->getSize() - 1 }, child);
                }
            }
        }
    }

    std::vector<ptrn::Pattern *> PatternLanguage::getPatternsAtAddress(u64 address, u64 section) const {
        if (this->m_flattenedPatterns.empty() || !this->m_flattenedPatterns.contains(section))
            return { };

        auto intervals = this->m_flattenedPatterns.at(section).overlapping({ address, address });

        std::vector<ptrn::Pattern*> results;
        std::transform(intervals.begin(), intervals.end(), std::back_inserter(results), [](const auto &interval) {
            ptrn::Pattern* value = interval.value;

            value->setOffset(interval.interval.start);
            value->clearFormatCache();

            return value;
        });

        return results;
    }

    std::vector<u32> PatternLanguage::getColorsAtAddress(u64 address, u64 section) const {
        if (this->m_flattenedPatterns.empty() || !this->m_flattenedPatterns.contains(section))
            return { };

        auto intervals = this->m_flattenedPatterns.at(section).overlapping({ address, address });

        std::vector<u32> results;
        for (auto &[interval, pattern] : intervals) {
            if (pattern->getVisibility() != pl::ptrn::Visibility::Visible)
                continue;

            results.push_back(pattern->getColor());
        }

        return results;
    }

}