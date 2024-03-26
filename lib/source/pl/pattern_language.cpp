#include <pl/pattern_language.hpp>

#include <pl/core/preprocessor.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/core/errors/error.hpp>
#include <pl/core/resolver.hpp>
#include <pl/core/resolvers.hpp>

#include <pl/patterns/pattern.hpp>

#include <pl/lib/std/libstd.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

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

        this->reset();
    }

    PatternLanguage::~PatternLanguage() {
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();
    }

    PatternLanguage::PatternLanguage(PatternLanguage &&other) noexcept {
        this->m_internals           = std::move(other.m_internals);
        other.m_internals = { };

        this->m_compileErrors       = std::move(other.m_compileErrors);
        this->m_currError           = std::move(other.m_currError);

        this->m_defines              = std::move(other.m_defines);
        this->m_pragmas             = std::move(other.m_pragmas);

        this->m_resolvers           = std::move(other.m_resolvers);
        this->m_fileResolver         = std::move(other.m_fileResolver);
        this->m_parserManager       = std::move(other.m_parserManager);

        this->m_patterns            = std::move(other.m_patterns);
        this->m_flattenedPatterns    = std::move(other.m_flattenedPatterns);
        this->m_cleanupCallbacks    = std::move(other.m_cleanupCallbacks);
        this->m_currAST             = std::move(other.m_currAST);

        this->m_running.exchange(other.m_running.load());
        this->m_patternsValid.exchange(other.m_patternsValid.load());
        this->m_aborted.exchange(other.m_aborted.load());
        this->m_runId.exchange(other.m_runId.load());

        m_startAddress  = std::move(other.m_startAddress);
        m_defaultEndian = other.m_defaultEndian;
        m_runningTime   = other.m_runningTime;
    }

    [[nodiscard]] std::optional<std::vector<pl::core::Token>> PatternLanguage::preprocessString(const std::string& code, const std::string& source) {
        this->reset();

        auto internalSource = addVirtualSource(code, source); // add virtual source to file resolver

        // add defines to preprocessor
        for (const auto &[name, value] : this->m_defines)
            this->m_internals.preprocessor->addDefine(name, value);

        // add pragmas to preprocessor
        for (const auto &[name, callback] : this->m_pragmas)
            this->m_internals.preprocessor->addPragmaHandler(name, callback);
        auto [tokens, preprocessorErrors] = this->m_internals.preprocessor->preprocess(this, internalSource, true);
        if (!preprocessorErrors.empty()) 
            this->m_compileErrors = std::move(preprocessorErrors);
        if (!tokens.has_value() || tokens->empty())
            return std::nullopt;
        return tokens;
    }

    std::optional<std::vector<std::shared_ptr<core::ast::ASTNode>>> PatternLanguage::parseString(const std::string &code, const std::string &source) {
        auto tokens = this->preprocessString(code, source);
        if (!tokens.has_value() || tokens->empty())
            return std::nullopt;

        auto [ast, parserErrors] = this->m_internals.parser->parse(tokens.value());
        if (!parserErrors.empty())
            this->m_compileErrors = std::move(parserErrors);

        if (!ast.has_value() || ast->empty())
            return std::nullopt;

        auto [validated, validatorErrors] = this->m_internals.validator->validate(ast.value());
        wolv::util::unused(validated);
        if (!validatorErrors.empty())
            this->m_compileErrors = std::move(validatorErrors);

        if (ast->empty() || !ast.has_value())
            return std::nullopt;

        return ast;
    }

    bool PatternLanguage::executeString(std::string code, const std::string& source, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
        const auto startTime = std::chrono::high_resolution_clock::now();
        ON_SCOPE_EXIT {
            const auto endTime = std::chrono::high_resolution_clock::now();
            this->m_runningTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime).count();
        };

        code = wolv::util::replaceStrings(code, "\r\n", "\n");
        code = wolv::util::replaceStrings(code, "\t", "    ");

        const auto &evaluator = this->m_internals.evaluator;

        this->m_running = true;
        this->m_aborted = false;
        this->m_runId += 1;
        ON_SCOPE_EXIT { this->m_running = false; };

        ON_SCOPE_EXIT {
            for (const auto &error: this->m_compileErrors) {
                evaluator->getConsole().log(core::LogConsole::Level::Error, error.format());
            }

            if (this->m_currError.has_value()) {
                const auto &error = this->m_currError.value();

                evaluator->getConsole().log(core::LogConsole::Level::Error, error.message);
            }

            for (const auto &cleanupCallback : this->m_cleanupCallbacks)
                cleanupCallback(*this);
        };

        evaluator->setInVariables(inVariables);

        for (const auto &[name, value] : envVars)
            evaluator->setEnvVariable(name, value);

        auto ast = this->parseString(code, source);
        if (!ast.has_value())
            return false;

        this->m_currAST = std::move(*ast);

        evaluator->setReadOffset(this->m_startAddress.value_or(evaluator->getDataBaseAddress()));

        if (!evaluator->evaluate(this->m_currAST)) {
            auto &console = evaluator->getConsole();

            this->m_currError = console.getLastHardError();

            console.log(core::LogConsole::Level::Error, "[ Stack Trace ]");

            const auto &callStack = evaluator->getCallStack();
            u32 lastLine = 0;
            for (const auto &entry : callStack | std::views::reverse) {
                if (entry == nullptr)
                    continue;

                auto location = entry->getLocation();
                if (lastLine == location.line)
                    continue;

                console.log(core::LogConsole::Level::Error, core::err::impl::formatLocation(location));
                console.log(core::LogConsole::Level::Error, core::err::impl::formatLines(location));
                lastLine = location.line;
            }
            console.log(core::LogConsole::Level::Error, "\n");

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

        auto success = this->executeString(functionContent, api::Source::DefaultSource, {}, {}, false);
        auto result  = this->m_internals.evaluator->getMainResult();

        return { success, std::move(result) };
    }

    api::Source* PatternLanguage::addVirtualSource(const std::string &code, const std::string &source) const {
        return this->m_fileResolver.addVirtualFile(code, source);
    }

    void PatternLanguage::abort() {
        this->m_internals.evaluator->abort();
        this->m_aborted = true;
    }

    void PatternLanguage::setIncludePaths(const std::vector<std::fs::path>& paths) const {
        this->m_fileResolver.setIncludePaths(paths);
    }

    void PatternLanguage::setResolver(const core::Resolver& resolver) {
        this->m_resolvers = resolver;
    }

    void PatternLanguage::addPragma(const std::string &name, const api::PragmaHandler &callback) {
        this->m_pragmas[name] = callback;
    }

    void PatternLanguage::removePragma(const std::string &name) {
        this->m_pragmas.erase(name);
    }

    void PatternLanguage::addDefine(const std::string &name, const std::string &value) {
        this->m_defines[name] = value;
    }

    void PatternLanguage::removeDefine(const std::string &name) {
        this->m_defines.erase(name);
    }

    void PatternLanguage::setDataSource(u64 baseAddress, u64 size, std::function<void(u64, u8*, size_t)> readFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writeFunction) const {
        this->m_internals.evaluator->setDataSource(baseAddress, size, std::move(readFunction), std::move(writeFunction));
    }

    const std::atomic<u64>& PatternLanguage::getLastReadAddress() const {
        return this->m_internals.evaluator->getLastReadAddress();
    }

    const std::atomic<u64>& PatternLanguage::getLastWriteAddress() const {
        return this->m_internals.evaluator->getLastWriteAddress();
    }

    const std::atomic<u64>& PatternLanguage::getLastPatternPlaceAddress() const {
        return this->m_internals.evaluator->getLastPatternPlaceAddress();
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

    const std::vector<core::err::CompileError>& PatternLanguage::getCompileErrors() const {
        return this->m_compileErrors;
    }


    u64 PatternLanguage::getCreatedPatternCount() const {
        return this->m_internals.evaluator->getPatternCount();
    }

    u64 PatternLanguage::getMaximumPatternCount() const {
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
        this->m_compileErrors.clear();
        this->m_parserManager.reset();
        this->m_internals.validator->setRecursionDepth(32);

        this->m_internals.evaluator->getConsole().clear();
        this->m_internals.evaluator->setDefaultEndian(this->m_defaultEndian);
        this->m_internals.evaluator->setEvaluationDepth(32);
        this->m_internals.evaluator->setArrayLimit(0x10000);
        this->m_internals.evaluator->setPatternLimit(0x20000);
        this->m_internals.evaluator->setLoopLimit(0x1000);
        this->m_internals.evaluator->setDebugMode(false);
        this->m_internals.parser->setParserManager(&m_parserManager);
        this->m_patternsValid = false;

        this->m_resolvers.setDefaultResolver([this](const std::string& path) {
            return this->m_fileResolver.resolve(path);
        });

        auto resolver = [this](const std::string& path) {
            return this->m_resolvers.resolve(path);
        };

        this->m_internals.preprocessor->setResolver(resolver);
        this->m_parserManager.setResolver(resolver);
        this->m_parserManager.setPatternLanguage(this);
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