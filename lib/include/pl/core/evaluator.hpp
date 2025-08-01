#pragma once

#include <atomic>
#include <bit>
#include <list>
#include <map>
#include <optional>
#include <vector>
#include <memory>
#include <set>
#include <unordered_set>
#include <unordered_map>

#include <pl/core/log_console.hpp>
#include <pl/core/token.hpp>
#include <pl/api.hpp>

#include <pl/core/errors/runtime_errors.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <pl/pattern_language.hpp>

namespace pl {
    class PatternLanguage;
}

namespace pl::ptrn {

    class Pattern;
    class PatternCreationLimiter;
    class PatternBitfieldField;

}

namespace pl::core {

    namespace ast {
        class ASTNode;
        class ASTNodeBitfieldField;
        class ASTNodeTypeDecl;
    }

    enum class DangerousFunctionPermission {
        Ask,
        Deny,
        Allow
    };

    struct ByteAndBitOffset {
        u64 byteOffset;
        u8 bitOffset;
    };

    class Evaluator {
    public:
        Evaluator() = default;

        [[nodiscard]] bool evaluate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast);

        [[nodiscard]] const auto &getPatterns() const {
            return this->m_patterns;
        }

        void addPattern(std::shared_ptr<ptrn::Pattern> &&pattern) {
            this->m_patterns.push_back(std::move(pattern));
        }

        [[nodiscard]] LogConsole &getConsole() {
            return this->m_console;
        }

        struct ParameterPack {
            std::string name;
            std::vector<Token::Literal> values;
        };

        struct Scope {
            Scope(const std::shared_ptr<pl::ptrn::Pattern>& parentPattern,
                std::vector<std::shared_ptr<pl::ptrn::Pattern>>* scopePatterns,
                size_t heapSize)
                : parent(parentPattern),
                scope(scopePatterns),
                heapStartSize(heapSize) {}

            std::shared_ptr<ptrn::Pattern> parent;
            std::vector<std::shared_ptr<ptrn::Pattern>> *scope;
            std::optional<ParameterPack> parameterPack;
            size_t heapStartSize;
        };

        struct PatternLocalData {
            u32 referenceCount;
            std::vector<u8> data;
        };

        struct UpdateHandler {
            UpdateHandler(Evaluator *evaluator, const ast::ASTNode *node);
            ~UpdateHandler();

            Evaluator *evaluator;
        };

        struct StackTrace {
            std::unique_ptr<ast::ASTNode> node;
            u64 cursorAddress;
        };

        void pushScope(const std::shared_ptr<ptrn::Pattern> &parent, std::vector<std::shared_ptr<ptrn::Pattern>> &scope);
        void popScope();

        [[nodiscard]] Scope &getScope(i32 index) {
            return *this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        [[nodiscard]] const Scope &getScope(i32 index) const {
            return *this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        [[nodiscard]] Scope &getGlobalScope() {
            return *this->m_scopes.front();
        }

        [[nodiscard]] const Scope &getGlobalScope() const {
            return *this->m_scopes.front();
        }

        [[nodiscard]] size_t getScopeCount() const {
            return this->m_scopes.size();
        }

        [[nodiscard]] bool isGlobalScope() const {
            return this->m_scopes.size() == 1;
        }

        [[nodiscard]] const std::vector<StackTrace>& getCallStack() const {
            return this->m_callStack;
        }

        void pushTemplateParameters() {
            if (this->m_templateParameters.empty())
                this->m_templateParameters.emplace_back();
            else
                this->m_templateParameters.push_back(this->m_templateParameters.back());
        }

        void popTemplateParameters() {
            this->m_templateParameters.pop_back();
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>>& getTemplateParameters() const {
            return this->m_templateParameters.back();
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>>& getTemplateParameters() {
            return this->m_templateParameters.back();
        }

        void pushSectionId(u64 id);
        void popSectionId();
        [[nodiscard]] u64 getSectionId() const;
        [[nodiscard]] u64 createSection(const std::string &name);
        void removeSection(u64 id);
        [[nodiscard]] std::vector<u8>& getSection(u64 id);
        [[nodiscard]] u64 getSectionSize(u64 id);
        [[nodiscard]] const std::map<u64, api::Section>& getSections() const;

        [[nodiscard]] u64 getSectionCount() const;

        void setInVariables(const std::map<std::string, Token::Literal> &inVariables) {
            this->m_inVariables = inVariables;
        }

        [[nodiscard]] std::map<std::string, Token::Literal> getOutVariables() const;

        void setDataSource(u64 baseAddress, size_t dataSize, std::function<void(u64, u8*, size_t)> readerFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writerFunction = std::nullopt);

        void setDataBaseAddress(u64 baseAddress) {
            this->m_dataBaseAddress = baseAddress;
        }

        void setDataSize(u64 dataSize) {
            this->m_dataSize = dataSize;
        }

        [[nodiscard]] u64 getDataBaseAddress() const {
            return this->m_dataBaseAddress;
        }

        [[nodiscard]] u64 getDataSize() const {
            return this->m_dataSize;
        }

        void accessData(u64 address, void *buffer, size_t size, u64 sectionId, bool write);
        void readData(u64 address, void *buffer, size_t size, u64 sectionId) {
            this->accessData(address, buffer, size, sectionId, false);
        }
        void writeData(u64 address, void *buffer, size_t size, u64 sectionId) {
            this->accessData(address, buffer, size, sectionId, true);
        }

        void setDefaultEndian(std::endian endian) {
            this->m_defaultEndian = endian;
        }

        [[nodiscard]] std::endian getDefaultEndian() const {
            return this->m_defaultEndian;
        }

        void setEvaluationDepth(u64 evalDepth) {
            this->m_evalDepth = evalDepth;
        }

        [[nodiscard]] u64 getEvaluationDepth() const {
            return this->m_evalDepth;
        }

        void setArrayLimit(u64 arrayLimit) {
            this->m_arrayLimit = arrayLimit;
        }

        [[nodiscard]] u64 getArrayLimit() const {
            return this->m_arrayLimit;
        }

        void setPatternLimit(u64 limit) {
            this->m_patternLimit = limit;
        }

        [[nodiscard]] u64 getPatternLimit() const {
            return this->m_patternLimit;
        }

        [[nodiscard]] u64 getPatternCount() const {
            return this->m_currPatternCount;
        }

        void setLoopLimit(u64 limit) {
            this->m_loopLimit = limit;
        }

        [[nodiscard]] u64 getLoopLimit() const {
            return this->m_loopLimit;
        }

        void alignToByte();
        u64 getReadOffset() const;
        u64 getReadOffsetAndIncrement(u64 incrementSize);

        void setReadOffset(u64 offset);
        void setStartAddress(u64 address);
        u64 getStartAddress() const;

        void setReadOrderReversed(bool reversed) { this->m_readOrderReversed = reversed; }

        [[nodiscard]] bool isReadOrderReversed() const { return this->m_readOrderReversed; }

        ByteAndBitOffset getBitwiseReadOffset() const { return { this->m_currOffset, static_cast<u8>(this->m_currBitOffset) }; }

        [[nodiscard]] ByteAndBitOffset getBitwiseReadOffsetAndIncrement(i128 bitSize);

        void setBitwiseReadOffset(u64 byteOffset, u8 bitOffset) {
            this->m_currOffset = byteOffset;
            this->m_currBitOffset = bitOffset;
        }

        void setBitwiseReadOffset(ByteAndBitOffset offset) {
            setBitwiseReadOffset(offset.byteOffset, offset.bitOffset);
        }

        [[nodiscard]] u128 readBits(u128 byteOffset, u8 bitOffset, u64 bitSize, u64 section, std::endian endianness);
        void writeBits(u128 byteOffset, u8 bitOffset, u64 bitSize, u64 section, std::endian endianness, u128 value);

        bool addBuiltinFunction(const std::string &name, api::FunctionParameterCount numParams, std::vector<Token::Literal> defaultParameters, const api::FunctionCallback &function, bool dangerous);
        bool addCustomFunction(const std::string &name, api::FunctionParameterCount numParams, std::vector<Token::Literal> defaultParameters, const api::FunctionCallback &function);

        [[nodiscard]] const std::unordered_map<std::string, api::Function> &getBuiltinFunctions() const {
            return this->m_builtinFunctions;
        }

        [[nodiscard]] const std::unordered_map<std::string, api::Function> &getCustomFunctions() const {
            return this->m_customFunctions;
        }

        [[nodiscard]] std::optional<api::Function> findFunction(const std::string &name) const;

        [[nodiscard]] std::vector<std::vector<u8>> &getHeap() {
            return this->m_heap;
        }

        [[nodiscard]] const std::vector<std::vector<u8>> &getHeap() const {
            return this->m_heap;
        }

        [[nodiscard]] std::map<u32, PatternLocalData> &getPatternLocalStorage() {
            return this->m_patternLocalStorage;
        }

        [[nodiscard]] const std::map<u32, PatternLocalData> &getPatternLocalStorage() const {
            return this->m_patternLocalStorage;
        }

        void createParameterPack(const std::string &name, const std::vector<Token::Literal> &values);

        void createArrayVariable(const std::string &name, const ast::ASTNode *type, size_t entryCount, u64 section, bool constant = false);
        std::shared_ptr<ptrn::Pattern> createVariable(const std::string &name, const pl::core::Location &loc, const ast::ASTNodeTypeDecl *type, const std::optional<Token::Literal> &value = std::nullopt, bool outVariable = false, bool reference = false, bool templateVariable = false, bool constant = false);
        std::shared_ptr<ptrn::Pattern>& getVariableByName(const std::string &name);
        void setVariable(const std::string &name, const Token::Literal &value);
        void setVariable(std::shared_ptr<ptrn::Pattern> &pattern, const Token::Literal &value);
        void setVariableAddress(const std::string &variableName, u64 address, u64 section = 0);
        void changePatternSection(ptrn::Pattern *pattern, u64 section);
        void changePatternType(std::shared_ptr<ptrn::Pattern> &pattern, std::shared_ptr<ptrn::Pattern> &&newPattern) const;

        void abort() {
            this->m_aborted = true;
            for (auto &subRuntime : m_subRuntimes) {
                subRuntime.abort();
            }
        }

        void handleAbort() const {
            if (this->m_aborted) [[unlikely]] {
                if (std::uncaught_exceptions() == 0) {
                    err::E0007.throwError("Evaluation aborted by user.");
                }
            }
        }

        [[nodiscard]] std::optional<Token::Literal> getEnvVariable(const std::string &name) const {
            if (this->m_envVariables.contains(name)) {
                return this->m_envVariables.at(name);
            } else
                return std::nullopt;
        }

        void setEnvVariable(const std::string &name, const Token::Literal &value) {
            this->m_envVariables[name] = value;
        }

        void setDangerousFunctionCallHandler(std::function<bool()> callback) {
            this->m_dangerousFunctionCalledCallback = std::move(callback);
        }

        void dangerousFunctionCalled() {
            this->allowDangerousFunctions(this->m_dangerousFunctionCalledCallback());
        }

        void allowDangerousFunctions(bool allow) {
            this->m_allowDangerousFunctions = allow ? DangerousFunctionPermission::Allow : DangerousFunctionPermission::Deny;
        }

        [[nodiscard]] DangerousFunctionPermission getDangerousFunctionPermission() const {
            return this->m_allowDangerousFunctions;
        }

        void setCurrentControlFlowStatement(ControlFlowStatement statement) {
            this->m_currControlFlowStatement = statement;
        }

        [[nodiscard]] ControlFlowStatement getCurrentControlFlowStatement() const {
            return this->m_currControlFlowStatement;
        }

        void setMainResult(Token::Literal result) {
            this->m_mainResult = std::move(result);
        }

        [[nodiscard]] const std::optional<Token::Literal> &getMainResult() {
            return this->m_mainResult;
        }

        void setCurrentArrayIndex(u64 index) { this->m_currArrayIndex = index; }
        void clearCurrentArrayIndex() { this->m_currArrayIndex = std::nullopt; }
        [[nodiscard]] std::optional<u64> getCurrentArrayIndex() const { return this->m_currArrayIndex; }

        void setDebugMode(bool enabled) {
            this->m_debugMode = enabled;

            if (enabled)
                this->m_console.setLogLevel(LogConsole::Level::Debug);
            else
                this->m_console.setLogLevel(LogConsole::Level::Info);
        }

        bool isDebugModeEnabled() const {
            return this->m_debugMode;
        }

        void allowMainSectionEdits() {
            this->m_mainSectionEditsAllowed = true;
        }

        [[nodiscard]] Evaluator::UpdateHandler updateRuntime(const ast::ASTNode *node);

        void addBreakpoint(u32 line);
        void removeBreakpoint(u32 line);
        void clearBreakpoints();
        void setBreakpointHitCallback(const std::function<void()> &callback);
        void setBreakpoints(const std::unordered_set<u32>& breakpoints);
        const std::unordered_set<u32>& getBreakpoints() const;
        void pauseNextLine();
        std::optional<u32> getPauseLine() const;

        const std::atomic<u64>& getLastReadAddress() const {
            return this->m_lastReadAddress;
        }

        const std::atomic<u64>& getLastWriteAddress() const {
            return this->m_lastWriteAddress;
        }

        const std::atomic<u64>& getLastPatternPlaceAddress() const {
            return this->m_lastPatternAddress;
        }

        std::set<std::string>& getStringPool() {
            return this->m_stringPool;
        }

        const std::set<std::string>& getStringPool() const {
            return this->m_stringPool;
        }

        bool isStringPoolEntryValid(const std::set<std::string>::const_iterator &it) const {
            return it != this->m_stringPool.end();
        }

        PatternLanguage& createSubRuntime() {
            return m_subRuntimes.emplace_back(this->m_patternLanguage->cloneRuntime());
        }

        PatternLanguage& getRuntime() {
            return *m_patternLanguage;
        }

        void setPatternColorPalette(std::span<const u32> palette) {
            m_patternColorPalette.clear();
            m_patternColorPalette.reserve(palette.size());

            std::ranges::copy(palette, std::back_inserter(m_patternColorPalette));
            resetPatternColorPaletteIndex();
        }

        void resetPatternColorPaletteIndex() {
            m_patternColorPaletteIndex = 0;
        }

        const std::set<ptrn::Pattern*>& getPatternsWithAttribute(const std::string &attribute) const {
            if (const auto it = m_attributedPatterns.find(attribute); it != m_attributedPatterns.end()) {
                return it->second;
            } else {
                static const std::set<ptrn::Pattern*> empty;
                return empty;
            }
        }

    private:
        void patternCreated(ptrn::Pattern *pattern);
        void patternDestroyed(ptrn::Pattern *pattern);

        api::FunctionCallback handleDangerousFunctionCall(const std::string &functionName, const api::FunctionCallback &function);

        void setRuntime(PatternLanguage *runtime) {
            this->m_patternLanguage = runtime;
        }

        std::optional<std::string> findTypeName(const ast::ASTNodeTypeDecl *type);

        void addAttributedPattern(const std::string &attribute, ptrn::Pattern *pattern) {
            m_attributedPatterns[attribute].insert(pattern);
        }

        void removeAttributedPattern(const std::string &attribute, ptrn::Pattern *pattern) {
            if (const auto it = m_attributedPatterns.find(attribute); it != m_attributedPatterns.end())
                it->second.erase(pattern);
        }

    private:
        PatternLanguage *m_patternLanguage = nullptr;
        std::list<PatternLanguage> m_subRuntimes;

        u64 m_currOffset = 0x00;
        i8 m_currBitOffset = 0;
        bool m_readOrderReversed = false;
        u64 m_startAddress = 0x00;

        bool m_evaluated = false;
        bool m_debugMode = false;
        LogConsole m_console;

        std::endian m_defaultEndian = std::endian::native;
        u64 m_evalDepth = 0;
        u64 m_arrayLimit = 0;
        u64 m_patternLimit = 0;
        u64 m_loopLimit = 0;

        std::atomic<u64> m_currPatternCount = 0;

        std::atomic<bool> m_aborted;

        std::vector<u64> m_sectionIdStack;
        std::map<u64, api::Section> m_sections;
        u64 m_sectionId = 0;

        std::vector<std::vector<u8>> m_heap;
        std::map<u32, PatternLocalData> m_patternLocalStorage;

        std::map<std::string, std::set<ptrn::Pattern*>> m_attributedPatterns;
        std::vector<std::unique_ptr<Scope>> m_scopes;
        std::vector<std::shared_ptr<ptrn::Pattern>> m_patterns;

        std::unordered_map <std::string, api::Function> m_customFunctions;
        std::unordered_map <std::string, api::Function> m_builtinFunctions;
        std::vector<std::unique_ptr<ast::ASTNode>> m_customFunctionDefinitions;

        std::optional<Token::Literal> m_mainResult;

        std::map<std::string, Token::Literal> m_envVariables;
        std::map<std::string, Token::Literal> m_inVariables;
        std::map<std::string, std::shared_ptr<ptrn::Pattern>> m_outVariables;
        std::map<std::string, Token::Literal> m_outVariableValues;
        std::vector<std::vector<std::shared_ptr<ptrn::Pattern>>> m_templateParameters;

        std::function<bool()> m_dangerousFunctionCalledCallback = []{ return false; };
        std::function<void()> m_breakpointHitCallback = []{ };
        std::atomic<DangerousFunctionPermission> m_allowDangerousFunctions = DangerousFunctionPermission::Ask;
        ControlFlowStatement m_currControlFlowStatement = ControlFlowStatement::None;
        std::vector<StackTrace> m_callStack;

        std::set<std::string> m_stringPool;

        u64 m_dataBaseAddress = 0x00;
        u64 m_dataSize = 0x00;
        std::function<void(u64, u8*, size_t)> m_readerFunction = [](u64, u8*, size_t){
            err::E0011.throwError("No memory has been attached. Reading is disabled.");
        };
        std::function<void(u64, const u8*, size_t)> m_writerFunction = [](u64, const u8*, size_t){
            err::E0011.throwError("No memory has been attached. Writing is disabled.");
        };

        bool m_mainSectionEditsAllowed = false;

        std::optional<u64> m_currArrayIndex;

        std::unordered_set<u32> m_breakpoints;
        std::optional<u32> m_lastPauseLine;
        bool m_shouldPauseNextLine = false;

        std::atomic<u64> m_lastReadAddress, m_lastWriteAddress, m_lastPatternAddress;
        std::atomic<ptrn::Pattern*> m_lastPattern = nullptr;
        std::vector<u32> m_sourceLineLength;

        constexpr static std::array<u32, 9> DefaultPatternColorPalette = { 0x70B4771F, 0x700E7FFF, 0x702CA02C, 0x702827D6, 0x70BD6794, 0x704B568C, 0x70C277E3, 0x7022BDBC, 0x70CFBE17 };
        std::vector<u32> m_patternColorPalette = { DefaultPatternColorPalette.begin(), DefaultPatternColorPalette.end() };
        u32 m_patternColorPaletteIndex = 0;

        u32 getNextPatternColor() {
            const auto index = this->m_patternColorPaletteIndex;
            this->m_patternColorPaletteIndex = (index + 1) % m_patternColorPalette.size();

            return m_patternColorPalette[index];
        }

        friend class pl::PatternLanguage;
        friend class pl::ptrn::PatternCreationLimiter;
        friend class pl::ptrn::Pattern;
        friend struct UpdateHandler;
    };

}
