#pragma once

#include <atomic>
#include <bit>
#include <map>
#include <optional>
#include <vector>
#include <memory>

#include <pl/log_console.hpp>
#include <pl/token.hpp>
#include <pl/api.hpp>

#include <fmt/format.h>

namespace pl {

    enum class DangerousFunctionPermission
    {
        Ask,
        Deny,
        Allow
    };

    enum class ControlFlowStatement
    {
        None,
        Continue,
        Break,
        Return
    };

    enum class BitfieldOrder
    {
        RightToLeft,
        LeftToRight
    };

    class Pattern;
    class PatternCreationLimiter;
    class ASTNode;

    class Evaluator {
    public:
        Evaluator() = default;

        std::optional<std::vector<std::shared_ptr<Pattern>>> evaluate(const std::vector<std::shared_ptr<ASTNode>> &ast);

        [[nodiscard]] LogConsole &getConsole() {
            return this->m_console;
        }

        struct ParameterPack {
            std::string name;
            std::vector<Token::Literal> values;
        };

        struct Scope {
            Pattern *parent;
            std::vector<std::shared_ptr<Pattern>> *scope;
            std::optional<ParameterPack> parameterPack;
        };

        void pushScope(Pattern *parent, std::vector<std::shared_ptr<Pattern>> &scope) {
            if (this->m_scopes.size() > this->getEvaluationDepth())
                LogConsole::abortEvaluation(fmt::format("evaluation depth exceeded set limit of {}", this->getEvaluationDepth()));

            this->handleAbort();

            this->m_scopes.push_back({ parent, &scope, { } });
        }

        void popScope() {
            this->m_scopes.pop_back();
        }

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

        void setInVariables(const std::map<std::string, Token::Literal> &inVariables) {
            this->m_inVariables = inVariables;
        }

        [[nodiscard]] std::map<std::string, Token::Literal> getOutVariables() const {
            std::map<std::string, Token::Literal> result;

            for (const auto &[name, offset] : this->m_outVariables) {
                result.insert({ name, this->getStack()[offset] });
            }

            return result;
        }

        void setDataSource(std::function<void(u64, u8*, size_t)> readerFunction, u64 baseAddress, size_t dataSize) {
            this->m_readerFunction = std::move(readerFunction);
            this->m_dataBaseAddress = baseAddress;
            this->m_dataSize = dataSize;
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

        void readData(u64 address, void *buffer, size_t size) const {
            this->m_readerFunction(address, reinterpret_cast<u8*>(buffer), size);
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

        void setBitfieldOrder(BitfieldOrder order) {
            this->m_bitfieldOrder = order;
        }

        [[nodiscard]] BitfieldOrder getBitfieldOrder() {
            return this->m_bitfieldOrder;
        }

        u64 &dataOffset() { return this->m_currOffset; }

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

        [[nodiscard]] const std::map<std::string, api::Function> &getBuiltinFunctions() const {
            return this->m_builtinFunctions;
        }

        [[nodiscard]] const std::map<std::string, api::Function> &getCustomFunctions() const {
            return this->m_customFunctions;
        }

        [[nodiscard]] std::vector<Token::Literal> &getStack() {
            return this->m_stack;
        }

        [[nodiscard]] const std::vector<Token::Literal> &getStack() const {
            return this->m_stack;
        }

        void createParameterPack(const std::string &name, const std::vector<Token::Literal> &values);
        void createVariable(const std::string &name, ASTNode *type, const std::optional<Token::Literal> &value = std::nullopt, bool outVariable = false);
        void setVariable(const std::string &name, const Token::Literal &value);

        void abort() {
            this->m_aborted = true;
        }

        void handleAbort() {
            if (this->m_aborted)
                LogConsole::abortEvaluation("evaluation aborted by user");
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

        [[nodiscard]] const std::optional<Token::Literal> &getMainResult() {
            return this->m_mainResult;
        }

    private:
        void patternCreated();
        void patternDestroyed();

    private:
        u64 m_currOffset = 0x00;
        LogConsole m_console;

        u32 m_colorIndex = 0;

        std::endian m_defaultEndian = std::endian::native;
        u64 m_evalDepth = 0;
        u64 m_arrayLimit = 0;
        u64 m_patternLimit = 0;
        u64 m_loopLimit = 0;

        u64 m_currPatternCount = 0;

        std::atomic<bool> m_aborted;

        std::vector<Scope> m_scopes;
        std::map<std::string, api::Function> m_customFunctions;
        std::map<std::string, api::Function> m_builtinFunctions;
        std::vector<std::unique_ptr<ASTNode>> m_customFunctionDefinitions;
        std::vector<Token::Literal> m_stack;

        std::optional<Token::Literal> m_mainResult;

        std::map<std::string, Token::Literal> m_envVariables;
        std::map<std::string, Token::Literal> m_inVariables;
        std::map<std::string, size_t> m_outVariables;

        std::function<bool()> m_dangerousFunctionCalledCallback = []{ return false; };
        std::atomic<DangerousFunctionPermission> m_allowDangerousFunctions = DangerousFunctionPermission::Ask;
        ControlFlowStatement m_currControlFlowStatement = ControlFlowStatement::None;
        BitfieldOrder m_bitfieldOrder = BitfieldOrder::RightToLeft;

        u64 m_dataBaseAddress = 0x00;
        u64 m_dataSize = 0x00;
        std::function<void(u64, u8*, size_t)> m_readerFunction = [](u64, u8*, size_t){
            LogConsole::abortEvaluation("reading data has been disabled");
        };

        u32 getNextPatternColor() {
            constexpr static std::array Palette = { 0x70B4771F, 0x700E7FFF, 0x702CA02C, 0x702827D6, 0x70BD6794, 0x704B568C, 0x70C277E3, 0x707F7F7F, 0x7022BDBC, 0x70CFBE17 };

            auto index = this->m_colorIndex;
            this->m_colorIndex = (this->m_colorIndex + 1) % Palette.size();

            return Palette[index];
        }

        friend class PatternCreationLimiter;
        friend class Pattern;
    };

}