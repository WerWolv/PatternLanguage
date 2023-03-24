#pragma once

#include <atomic>
#include <bit>
#include <map>
#include <optional>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include <pl/core/log_console.hpp>
#include <pl/core/token.hpp>
#include <pl/api.hpp>

#include <fmt/format.h>

namespace pl::ptrn {

    class Pattern;
    class PatternCreationLimiter;
    class PatternBitfieldField;

}

namespace pl::core {

    namespace ast {
        class ASTNode;
        class ASTNodeBitfieldField;
    }

    enum class DangerousFunctionPermission {
        Ask,
        Deny,
        Allow
    };

    enum class ControlFlowStatement {
        None,
        Continue,
        Break,
        Return
    };

    enum class BitfieldOrder {
        RightToLeft,
        LeftToRight
    };

    class Evaluator {
    public:
        Evaluator() = default;

        [[nodiscard]] bool evaluate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast);

        [[nodiscard]] const auto &getPatterns() const {
            return this->m_patterns;
        }

        [[nodiscard]] LogConsole &getConsole() {
            return this->m_console;
        }

        struct ParameterPack {
            std::string name;
            std::vector<Token::Literal> values;
        };

        struct Scope {
            std::shared_ptr<ptrn::Pattern> parent;
            std::vector<std::shared_ptr<ptrn::Pattern>> *scope;
            std::optional<ParameterPack> parameterPack;
            std::vector<std::shared_ptr<ptrn::Pattern>> savedPatterns;
            size_t heapStartSize;
        };

        struct PatternLocalData {
            u32 referenceCount;
            std::vector<u8> data;
        };

        void pushScope(const std::shared_ptr<ptrn::Pattern> &parent, std::vector<std::shared_ptr<ptrn::Pattern>> &scope);
        void popScope();

        [[nodiscard]] Scope &getScope(i32 index) {
            return this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        [[nodiscard]] const Scope &getScope(i32 index) const {
            return this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        [[nodiscard]] Scope &getGlobalScope() {
            return this->m_scopes.front();
        }

        [[nodiscard]] const Scope &getGlobalScope() const {
            return this->m_scopes.front();
        }

        [[nodiscard]] size_t getScopeCount() {
            return this->m_scopes.size();
        }

        [[nodiscard]] bool isGlobalScope() {
            return this->m_scopes.size() == 1;
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

        [[nodiscard]] const std::vector<std::shared_ptr<ptrn::Pattern>>& getTemplateParameters() {
            return this->m_templateParameters.back();
        }

        void pushSectionId(u64 id);
        void popSectionId();
        [[nodiscard]] u64 getSectionId() const;
        [[nodiscard]] u64 createSection(const std::string &name);
        void removeSection(u64 id);
        [[nodiscard]] std::vector<u8>& getSection(u64 id);
        [[nodiscard]] const std::map<u64, api::Section>& getSections() const;

        [[nodiscard]] u64 getSectionCount() const;

        void setInVariables(const std::map<std::string, Token::Literal> &inVariables) {
            this->m_inVariables = inVariables;
        }

        [[nodiscard]] std::map<std::string, Token::Literal> getOutVariables() const;

        void setDataSource(u64 baseAddress, size_t dataSize, std::function<void(u64, u8*, size_t)> readerFunction, std::optional<std::function<void(u64, const u8*, size_t)>> writerFunction = std::nullopt) {
            this->m_dataBaseAddress = baseAddress;
            this->m_dataSize = dataSize;

            this->m_readerFunction = std::move(readerFunction);
            if (writerFunction.has_value()) this->m_writerFunction = std::move(writerFunction.value());
        }

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

        void setBitfieldOrder(std::optional<BitfieldOrder> order) {
            this->m_bitfieldOrder = order;
        }

        [[nodiscard]] std::optional<BitfieldOrder> getBitfieldOrder() {
            return this->m_bitfieldOrder;
        }

        u64 &dataOffset() { return this->m_currOffset; }

        u8 getBitfieldBitOffset() { return this->m_bitfieldBitOffset; }

        void addToBitfieldBitOffset(u128 bitSize) {
            this->dataOffset() += bitSize >> 3;
            this->m_bitfieldBitOffset += bitSize & 0x7;

            this->dataOffset() += this->m_bitfieldBitOffset >> 3;
            this->m_bitfieldBitOffset &= 0x7;
        }

        void resetBitfieldBitOffset() {
            if (m_bitfieldBitOffset != 0)
                this->dataOffset()++;
            this->m_bitfieldBitOffset = 0;
        }

        [[nodiscard]] u128 readBits(u128 byteOffset, u8 bitOffset, u64 bitSize, u64 section, std::endian endianness) {
            u128 value = 0;

            size_t readSize = (bitOffset + bitSize + 7) / 8;
            readSize = std::min(readSize, sizeof(value));
            this->readData(byteOffset, &value, readSize, section);
            value = hlp::changeEndianess(value, sizeof(value), endianness);

            size_t offset = endianness == std::endian::little ? bitOffset : (sizeof(value) * 8) - bitOffset - bitSize;
            auto mask = (u128(1) << bitSize) - 1;
            value = (value >> offset) & mask;
            return value;
        }

        bool addBuiltinFunction(const std::string &name, api::FunctionParameterCount numParams, std::vector<Token::Literal> defaultParameters, const api::FunctionCallback &function, bool dangerous) {
            const auto [iter, inserted] = this->m_builtinFunctions.insert({
                name, {numParams, std::move(defaultParameters), function, dangerous}
            });

            return inserted;
        }

        bool addCustomFunction(const std::string &name, api::FunctionParameterCount numParams, std::vector<Token::Literal> defaultParameters, const api::FunctionCallback &function) {
            const auto [iter, inserted] = this->m_customFunctions.insert({
                name, {numParams, std::move(defaultParameters), function, false}
            });

            return inserted;
        }

        [[nodiscard]] const std::unordered_map<std::string, api::Function> &getBuiltinFunctions() const {
            return this->m_builtinFunctions;
        }

        [[nodiscard]] const std::unordered_map<std::string, api::Function> &getCustomFunctions() const {
            return this->m_customFunctions;
        }

        [[nodiscard]] std::optional<api::Function> findFunction(const std::string &name) const {
            const auto &customFunctions     = this->getCustomFunctions();
            const auto &builtinFunctions    = this->getBuiltinFunctions();

            if (auto customFunction = customFunctions.find(name); customFunction != customFunctions.end())
                return customFunction->second;
            else if (auto builtinFunction = builtinFunctions.find(name); builtinFunction != builtinFunctions.end())
                return builtinFunction->second;
            else
                return std::nullopt;
        }

        [[nodiscard]] std::vector<std::vector<u8>> &getHeap() {
            return this->m_heap;
        }

        [[nodiscard]] const std::vector<std::vector<u8>> &getHeap() const {
            return this->m_heap;
        }

        void createParameterPack(const std::string &name, const std::vector<Token::Literal> &values);

        void createArrayVariable(const std::string &name, ast::ASTNode *type, size_t entryCount, u64 section, bool constant = false);
        std::shared_ptr<ptrn::Pattern> createVariable(const std::string &name, ast::ASTNode *type, const std::optional<Token::Literal> &value = std::nullopt, bool outVariable = false, bool reference = false, bool templateVariable = false, bool constant = false);
        std::shared_ptr<ptrn::Pattern>& getVariableByName(const std::string &name);
        void setVariable(const std::string &name, const Token::Literal &value);
        void setVariable(ptrn::Pattern *pattern, const Token::Literal &value);
        void setVariableAddress(const std::string &variableName, u64 address, u64 section = 0);

        void abort() {
            this->m_aborted = true;
        }

        void handleAbort() {
            if (this->m_aborted)
                err::E0007.throwError("Evaluation aborted by user.");
        }

        [[nodiscard]] std::optional<Token::Literal> getEnvVariable(const std::string &name) const {
            if (this->m_envVariables.contains(name)) {
                auto value = this->m_envVariables.at(name);
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

        void updateRuntime(const ast::ASTNode *node);

        void addBreakpoint(u64 line);
        void removeBreakpoint(u64 line);
        void clearBreakpoints();
        void setBreakpointHitCallback(const std::function<void()> &callback);
        const std::unordered_set<int>& getBreakpoints() const;
        std::optional<u32> getPauseLine() const;

    private:
        void patternCreated(ptrn::Pattern *pattern);
        void patternDestroyed(ptrn::Pattern *pattern);

    private:
        u64 m_currOffset = 0x00;

        bool m_evaluated = false;
        bool m_debugMode = false;
        LogConsole m_console;

        u32 m_colorIndex = 0;

        std::endian m_defaultEndian = std::endian::native;
        u64 m_evalDepth = 0;
        u64 m_arrayLimit = 0;
        u64 m_patternLimit = 0;
        u64 m_loopLimit = 0;

        u64 m_currPatternCount = 0;

        std::atomic<bool> m_aborted;

        std::vector<u64> m_sectionIdStack;
        std::map<u64, api::Section> m_sections;
        u64 m_sectionId = 0;

        std::vector<Scope> m_scopes;
        std::unordered_map <std::string, api::Function> m_customFunctions;
        std::unordered_map <std::string, api::Function> m_builtinFunctions;
        std::vector<std::unique_ptr<ast::ASTNode>> m_customFunctionDefinitions;

        std::optional<Token::Literal> m_mainResult;

        std::map<std::string, Token::Literal> m_envVariables;
        std::map<std::string, Token::Literal> m_inVariables;
        std::map<std::string, std::unique_ptr<ptrn::Pattern>> m_outVariables;
        std::vector<std::vector<std::shared_ptr<ptrn::Pattern>>> m_templateParameters;

        std::vector<std::vector<u8>> m_heap;
        std::map<u32, PatternLocalData> m_patternLocalStorage;

        std::function<bool()> m_dangerousFunctionCalledCallback = []{ return false; };
        std::function<void()> m_breakpointHitCallback = []{ };
        std::atomic<DangerousFunctionPermission> m_allowDangerousFunctions = DangerousFunctionPermission::Ask;
        ControlFlowStatement m_currControlFlowStatement = ControlFlowStatement::None;
        std::optional<BitfieldOrder> m_bitfieldOrder;
        u8 m_bitfieldBitOffset = 0;

        std::vector<std::shared_ptr<ptrn::Pattern>> m_patterns;

        u64 m_dataBaseAddress = 0x00;
        u64 m_dataSize = 0x00;
        std::function<void(u64, u8*, size_t)> m_readerFunction = [](u64, u8*, size_t){
            err::E0011.throwError("No memory has been attached. Reading is disabled.");
        };
        std::function<void(u64, u8*, size_t)> m_writerFunction = [](u64, u8*, size_t){
            err::E0011.throwError("No memory has been attached. Reading is disabled.");
        };

        std::optional<u64> m_currArrayIndex;

        std::unordered_set<int> m_breakpoints;
        std::optional<u32> m_lastPauseLine;

        u32 getNextPatternColor() {
            constexpr static std::array Palette = { 0x70B4771F, 0x700E7FFF, 0x702CA02C, 0x702827D6, 0x70BD6794, 0x704B568C, 0x70C277E3, 0x7022BDBC, 0x70CFBE17 };

            auto index = this->m_colorIndex;
            this->m_colorIndex = (this->m_colorIndex + 1) % Palette.size();

            return Palette[index];
        }

        friend class pl::ptrn::PatternCreationLimiter;
        friend class pl::ptrn::Pattern;
    };

}
