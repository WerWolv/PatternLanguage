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
#include <pl/patterns/pattern_array_static.hpp>

#include <pl/lib/std/libstd.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace pl {

    static std::string getFunctionName(const api::Namespace &ns, const std::string &name) {
        std::string functionName;


        for (auto &scope : ns)
            functionName += fmt::format("{}::", scope);

        functionName += name;

        return functionName;
    }

    PatternLanguage::PatternLanguage(const bool addLibStd) {
        this->m_internals = {
            .preprocessor   = std::make_unique<core::Preprocessor>(),
            .lexer          = std::make_unique<core::Lexer>(),
            .parser         = std::make_unique<core::Parser>(),
            .validator      = std::make_unique<core::Validator>(),
            .evaluator      = std::make_unique<core::Evaluator>()
        };

        this->m_internals.evaluator->setRuntime(this);

        if (addLibStd)
            lib::libstd::registerFunctions(*this);

        this->reset();
    }

    PatternLanguage::~PatternLanguage() {
        if (this->m_flattenThread.joinable())
            this->m_flattenThread.join();
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();
        this->m_flattenedPatternsValid = false;
    }

    PatternLanguage::PatternLanguage(PatternLanguage &&other) noexcept {
        if (this->m_flattenThread.joinable())
            this->m_flattenThread.join();

        this->m_internals           = std::move(other.m_internals);
        other.m_internals = { };
        this->m_internals.evaluator->setRuntime(this);


        this->m_compileErrors       = std::move(other.m_compileErrors);
        this->m_currError           = std::move(other.m_currError);

        this->m_defines              = std::move(other.m_defines);
        this->m_pragmas             = std::move(other.m_pragmas);

        this->m_resolvers           = std::move(other.m_resolvers);
        this->m_fileResolver        = std::move(other.m_fileResolver);
        this->m_parserManager       = std::move(other.m_parserManager);

        this->m_patterns            = std::move(other.m_patterns);
        this->m_flattenedPatterns   = std::move(other.m_flattenedPatterns);
        this->m_cleanupCallbacks    = std::move(other.m_cleanupCallbacks);
        this->m_currAST             = std::move(other.m_currAST);

        this->m_dataBaseAddress     = other.m_dataBaseAddress;
        this->m_dataSize            = other.m_dataSize;
        this->m_dataReadFunction    = std::move(other.m_dataReadFunction);
        this->m_dataWriteFunction   = std::move(other.m_dataWriteFunction);

        this->m_logCallback                     = std::move(other.m_logCallback);
        this->m_dangerousFunctionCallCallback   = std::move(other.m_dangerousFunctionCallCallback);

        this->m_functions           = std::move(other.m_functions);

        this->m_running.exchange(other.m_running.load());
        this->m_patternsValid.exchange(other.m_patternsValid.load());
        this->m_aborted.exchange(other.m_aborted.load());
        this->m_runId.exchange(other.m_runId.load());
        this->m_subRuntime = other.m_subRuntime;

        m_startAddress  = std::move(other.m_startAddress);
        m_defaultEndian = other.m_defaultEndian;
        m_runningTime   = other.m_runningTime;
    }

    PatternLanguage PatternLanguage::cloneRuntime() const {
        PatternLanguage runtime;
        runtime.m_defines       = this->m_defines;
        runtime.m_pragmas       = this->m_pragmas;

        runtime.m_resolvers     = this->m_resolvers;
        runtime.m_fileResolver  = this->m_fileResolver;
        runtime.m_parserManager = this->m_parserManager;

        runtime.m_startAddress  = this->m_startAddress;
        runtime.m_defaultEndian = this->m_defaultEndian;

        runtime.m_dataBaseAddress     = this->m_dataBaseAddress;
        runtime.m_dataSize            = this->m_dataSize;
        runtime.m_dataReadFunction    = this->m_dataReadFunction;
        runtime.m_dataWriteFunction   = this->m_dataWriteFunction;

        runtime.m_logCallback                     = this->m_logCallback;
        runtime.m_dangerousFunctionCallCallback   = this->m_dangerousFunctionCallCallback;

        runtime.m_functions = this->m_functions;
        runtime.m_subRuntime = true;

        return runtime;
    }

    [[nodiscard]] std::optional<std::vector<pl::core::Token>> PatternLanguage::preprocessString(const std::string& code, const std::string& source) {
        this->reset();

        auto internalSource = addVirtualSource(code, source, true); // add virtual source to file resolver

        // add defines to preprocessor
        for (const auto &[name, value] : this->m_defines)
            this->m_internals.preprocessor->addDefine(name, value);

        // add pragmas to preprocessor
        for (const auto &[name, callback] : this->m_pragmas)
            this->m_internals.preprocessor->addPragmaHandler(name, callback);
        this->m_compileErrors.clear();

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

        this->m_parserManager.setPreprocessorOnceIncluded(this->m_internals.preprocessor->getOnceIncludedFiles());
        this->m_internals.parser->setParserManager(&this->m_parserManager);
        auto [ast, parserErrors] = this->m_internals.parser->parse(tokens.value());
        if (!parserErrors.empty()) {
            this->m_compileErrors.insert(m_compileErrors.end(), parserErrors.begin(), parserErrors.end());
            parserErrors.clear();
        }

        this->m_internals.preprocessor->setOutput(tokens.value());

        if (!ast.has_value())
            return std::nullopt;
        if (ast->empty())
            return ast;


        auto [validated, validatorErrors] = this->m_internals.validator->validate(ast.value());
        wolv::util::unused(validated);
        if (!validatorErrors.empty()) {
            this->m_compileErrors.insert(m_compileErrors.end(), validatorErrors.begin(), validatorErrors.end());
            validatorErrors.clear();
        }


        this->m_internals.preprocessor->setStoredErrors(this->m_compileErrors);

        if (ast->empty() || !ast.has_value())
            return std::nullopt;
        this->m_currAST = std::move(*ast);
        return m_currAST;
    }

    bool PatternLanguage::executeString(const std::string &code, const std::string& source, const std::map<std::string, core::Token::Literal> &envVars, const std::map<std::string, core::Token::Literal> &inVariables, bool checkResult) {
	   	const auto startTime = std::chrono::high_resolution_clock::now();
        ON_SCOPE_EXIT {
            const auto endTime = std::chrono::high_resolution_clock::now();
            this->m_runningTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime).count();
        };

        const auto &evaluator = this->m_internals.evaluator;

        evaluator->getConsole().setLogCallback(this->m_logCallback);

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
        // do not continue execution if there are any compile errors
        if (!this->m_compileErrors.empty())
            return false;

        this->m_currAST = std::move(*ast);

        for (const auto &[ns, name, parameterCount, callback, dangerous] : this->m_functions) {
            this->m_internals.evaluator->addBuiltinFunction(getFunctionName(ns, name), parameterCount, { }, callback, dangerous);
        }

        std::optional<std::function<void(u64, const u8*, size_t)>> writeFunction;
        if (m_dataWriteFunction.has_value()) {
            writeFunction = [this](u64 address, const u8 *buffer, size_t size) {
                return (*this->m_dataWriteFunction)(address + this->getStartAddress(), buffer, size);
            };
        }

        evaluator->setDataSource(this->m_dataBaseAddress, this->m_dataSize,
            [this](u64 address, u8 *buffer, size_t size) {
                return this->m_dataReadFunction(address + this->getStartAddress(), buffer, size);
            },
            writeFunction
        );

        evaluator->setStartAddress(this->getStartAddress());
        evaluator->setReadOffset(evaluator->getDataBaseAddress());
        evaluator->setDangerousFunctionCallHandler(this->m_dangerousFunctionCallCallback);

        bool evaluationResult = evaluator->evaluate(this->m_currAST);
        if (!evaluationResult) {
            auto &console = evaluator->getConsole();

            this->m_currError = console.getLastHardError();

            console.log(core::LogConsole::Level::Error, "[ Stack Trace ]");

            const auto &callStack = evaluator->getCallStack();
            u32 lastLine = 0;
            for (const auto &entry : callStack | std::views::reverse) {
                const auto &[node, address] = entry;
                if (node == nullptr)
                    continue;

                auto location = node->getLocation();
                if (lastLine == location.line)
                    continue;

                console.log(core::LogConsole::Level::Error, core::err::impl::formatLocation(location, address));
                console.log(core::LogConsole::Level::Error, core::err::impl::formatLines(location));
                lastLine = location.line;
            }

            if (auto cursor = this->m_currError->cursorAddress; cursor.has_value())
                console.log(core::LogConsole::Level::Error, fmt::format("Error happened with cursor at address 0x{:04X}", *cursor + m_startAddress.value_or(0x00)));

            console.log(core::LogConsole::Level::Error, "\n");
        } else {
            auto returnCode = evaluator->getMainResult().value_or(i128(0)).toSigned();

            if (!isSubRuntime()) {
                evaluator->getConsole().log(core::LogConsole::Level::Info, fmt::format("Pattern exited with code: {}", i64(returnCode)));
            }

            if (checkResult && returnCode != 0) {
                this->m_currError = core::err::PatternLanguageError(core::err::E0009.format(fmt::format("Pattern exited with non-zero result: {}", i64(returnCode))), 0, 1);

                evaluationResult = false;
            }
        }

        for (const auto &pattern : evaluator->getPatterns())
            this->m_patterns[pattern->getSection()].push_back(pattern);

        for (const auto &pattern : this->m_patterns[ptrn::Pattern::HeapSectionId]) {
            if (pattern->hasAttribute("export")) {
                this->m_patterns[ptrn::Pattern::MainSectionId].emplace_back(pattern);
            }
        }


        if (this->m_aborted) {
            this->reset();
        } else {
            this->flattenPatterns();
            this->m_patternsValid = true;
        }

        return evaluationResult;
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

    api::Source* PatternLanguage::addVirtualSource(const std::string &code, const std::string &source, bool mainSource) const {
        return this->m_fileResolver.addVirtualFile(code, source, mainSource);
    }

    void PatternLanguage::abort() {
        this->m_internals.evaluator->abort();
        this->m_aborted = true;
    }

    void PatternLanguage::setIncludePaths(const std::vector<std::fs::path>& paths) {
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

    void PatternLanguage::setDataSource(u64 baseAddress, u64 size, std::function<void(u64, u8*, size_t)> readFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writeFunction) {
        this->m_dataBaseAddress = baseAddress;
        this->m_dataSize = size;
        this->m_dataReadFunction = std::move(readFunction);
        this->m_dataWriteFunction = std::move(writeFunction);
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

    void PatternLanguage::setDataBaseAddress(u64 baseAddress) {
        this->m_dataBaseAddress = baseAddress;
    }

    void PatternLanguage::setDataSize(u64 size) {
        this->m_dataSize = size;
    }

    void PatternLanguage::setDefaultEndian(std::endian endian) {
        this->m_defaultEndian = endian;
    }

    void PatternLanguage::setStartAddress(u64 address) {
        this->m_startAddress = address;
    }

    u64 PatternLanguage::getStartAddress() const {
        return this->m_startAddress.value_or(0x00);
    }


    void PatternLanguage::setDangerousFunctionCallHandler(std::function<bool()> callback) {
        this->m_dangerousFunctionCallCallback = std::move(callback);
    }

    [[nodiscard]] std::map<std::string, core::Token::Literal> PatternLanguage::getOutVariables() const {
        return this->m_internals.evaluator->getOutVariables();
    }


    void PatternLanguage::setLogCallback(const core::LogConsole::Callback &callback) {
        this->m_logCallback = callback;
    }

    const std::optional<core::err::PatternLanguageError> &PatternLanguage::getEvalError() const {
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
        if (this->m_flattenThread.joinable())
            this->m_flattenThread.join();
        this->m_patterns.clear();
        this->m_flattenedPatterns.clear();
        this->m_flattenedPatternsValid = false;

        this->m_currError.reset();
        this->m_compileErrors.clear();
        this->m_parserManager.reset();
        this->m_internals.validator->setRecursionDepth(32);

        this->m_internals.evaluator->getConsole().clear();
        this->m_internals.evaluator->setDefaultEndian(this->m_defaultEndian);
        this->m_internals.evaluator->setEvaluationDepth(32);
        this->m_internals.evaluator->setArrayLimit(0x10000);
        this->m_internals.evaluator->setPatternLimit(0x100000);
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

    void PatternLanguage::addFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) {
        this->m_functions.emplace_back(ns, name, parameterCount, func, false);
    }

    void PatternLanguage::addDangerousFunction(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::FunctionCallback &func) {
        this->m_functions.emplace_back(ns, name, parameterCount, func, true);
    }

    void PatternLanguage::addType(const api::Namespace &ns, const std::string &name, api::FunctionParameterCount parameterCount, const api::TypeCallback &func) {
        this->m_parserManager.addBuiltinType(getFunctionName(ns, name), parameterCount, func);
    }

    void PatternLanguage::flattenPatterns() {
        for (const auto &[section, patterns] : this->m_patterns) {
            if (this->m_aborted)
                return;

            auto &sectionTree = this->m_flattenedPatterns[section];
            for (const auto &pattern : patterns) {
                if (this->m_aborted)
                    return;

                if (auto staticArray = dynamic_cast<ptrn::PatternArrayStatic*>(pattern.get()); staticArray != nullptr) {
                    if (staticArray->getEntryCount() > 0 && staticArray->getEntry(0)->getChildren().empty()) {
                        const auto address = staticArray->getOffset();
                        const auto size = staticArray->getSize();
                        sectionTree.insert({ address, address + size - 1 }, staticArray);
                        continue;
                    }
                }

                auto children = pattern->getChildren();
                for (const auto &[address, child] : children) {
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

            auto parent = value->getParent();
            while (parent != nullptr && dynamic_cast<const ptrn::PatternArrayStatic*>(parent->getParent()) == nullptr) {
                parent = parent->getParent();
            }

            // Handle static array members
            if (parent != nullptr) {
                parent->setOffset(interval.interval.start - (value->getOffset() - parent->getOffset()));
                parent->clearFormatCache();
                value->clearFormatCache();
            }

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
            auto visibility = pattern->getVisibility();
            if (visibility == pl::ptrn::Visibility::Hidden || visibility == pl::ptrn::Visibility::HighlightHidden)
                continue;

            results.push_back(pattern->getColor());
        }

        return results;
    }

    const std::set<ptrn::Pattern*>& PatternLanguage::getPatternsWithAttribute(const std::string &attribute) const {
        return m_internals.evaluator->getPatternsWithAttribute(attribute);
    }

}