#pragma once

#include <pl/helpers/types.hpp>

#include <pl/core/token.hpp>
#include <pl/core/errors/parser_errors.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_rvalue.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pl::core {

    class Parser {
    public:
        using TokenIter = std::vector<Token>::const_iterator;

        Parser()  = default;
        ~Parser() = default;

        std::optional<std::vector<std::shared_ptr<ast::ASTNode>>> parse(const std::string &sourceCode, const std::vector<Token> &tokens);

        const std::optional<err::PatternLanguageError> &getError() { return this->m_error; }
        const auto &getTypes() { return this->m_types; }

    private:
        std::optional<err::PatternLanguageError> m_error;
        TokenIter m_curr;
        TokenIter m_originalPosition, m_partOriginalPosition;

        std::vector<std::shared_ptr<ast::ASTNodeTypeDecl>> m_currTemplateType;
        std::map<std::string, std::shared_ptr<ast::ASTNodeTypeDecl>> m_types;

        std::vector<TokenIter> m_matchedOptionals;
        std::vector<std::vector<std::string>> m_currNamespace;

        u32 getLine(i32 index) const {
            return this->m_curr[index].line;
        }

        u32 getColumn(i32 index) const {
            return this->m_curr[index].column;
        }

        template<typename T, typename...Ts>
        std::unique_ptr<T> create(Ts&&... ts) {
            auto temp = std::make_unique<T>(std::forward<Ts>(ts)...);
            temp->setSourceLocation(this->getLine(-1), this->getColumn(-1));
            return temp;
        }

        template<typename T, typename...Ts>
        std::shared_ptr<T> createShared(Ts&&... ts) {
            auto temp = std::make_shared<T>(std::forward<Ts>(ts)...);
            temp->setSourceLocation(this->getLine(-1), this->getColumn(-1));
            return temp;
        }

        template<typename T>
        const T &getValue(i32 index) {
            auto &token = this->m_curr[index];
            auto value = std::get_if<T>(&token.value);

            if (value == nullptr) {
                std::visit([&](auto &&value) {
                    err::P0001.throwError(fmt::format("Expected {}, got {}.", typeid(T).name(), typeid(value).name()), "This is a serious parsing bug. Please open an issue on GitHub!", -index);
                }, token.value);
            }

            return *value;
        }

        [[nodiscard]] std::string getFormattedToken(i32 index) const {
            return fmt::format("{} ({})", this->m_curr[index].getFormattedType(), this->m_curr[index].getFormattedValue());
        }

        std::vector<std::string> getNamespacePrefixedNames(const std::string &name) {
            std::vector<std::string> result;

            result.push_back(name);

            std::string namespacePrefix;
            for (const auto &part : this->m_currNamespace.back()) {
                namespacePrefix += fmt::format("{}::", part);
                result.push_back(fmt::format("{}{}", namespacePrefix, name));
            }

            return result;
        }

        std::unique_ptr<ast::ASTNode> parseFunctionCall();
        std::unique_ptr<ast::ASTNode> parseStringLiteral();
        std::string parseNamespaceResolution();
        std::unique_ptr<ast::ASTNode> parseScopeResolution();
        std::unique_ptr<ast::ASTNode> parseRValue();
        std::unique_ptr<ast::ASTNode> parseRValue(ast::ASTNodeRValue::Path &path);
        std::unique_ptr<ast::ASTNode> parseFactor();
        std::unique_ptr<ast::ASTNode> parseCastExpression();
        std::unique_ptr<ast::ASTNode> parseUnaryExpression();
        std::unique_ptr<ast::ASTNode> parseMultiplicativeExpression();
        std::unique_ptr<ast::ASTNode> parseAdditiveExpression();
        std::unique_ptr<ast::ASTNode> parseShiftExpression();
        std::unique_ptr<ast::ASTNode> parseBinaryAndExpression();
        std::unique_ptr<ast::ASTNode> parseBinaryXorExpression();
        std::unique_ptr<ast::ASTNode> parseBinaryOrExpression();
        std::unique_ptr<ast::ASTNode> parseBooleanAnd(bool inTemplate);
        std::unique_ptr<ast::ASTNode> parseBooleanXor(bool inTemplate);
        std::unique_ptr<ast::ASTNode> parseBooleanOr(bool inTemplate);
        std::unique_ptr<ast::ASTNode> parseRelationExpression(bool inTemplate);
        std::unique_ptr<ast::ASTNode> parseEqualityExpression(bool inTemplate);
        std::unique_ptr<ast::ASTNode> parseTernaryConditional(bool inTemplate);
        std::unique_ptr<ast::ASTNode> parseMathematicalExpression(bool inTemplate = false);

        std::unique_ptr<ast::ASTNode> parseFunctionDefinition();
        std::unique_ptr<ast::ASTNode> parseFunctionVariableDecl();
        std::unique_ptr<ast::ASTNode> parseFunctionStatement(bool needsSemicolon = true);
        std::unique_ptr<ast::ASTNode> parseFunctionVariableAssignment(const std::string &lvalue);
        std::unique_ptr<ast::ASTNode> parseFunctionVariableCompoundAssignment(const std::string &lvalue);
        std::unique_ptr<ast::ASTNode> parseFunctionControlFlowStatement();
        std::vector<std::unique_ptr<ast::ASTNode>> parseStatementBody();
        std::unique_ptr<ast::ASTNode> parseFunctionConditional();
        std::unique_ptr<ast::ASTNode> parseFunctionWhileLoop();
        std::unique_ptr<ast::ASTNode> parseFunctionForLoop();

        void parseAttribute(ast::Attributable *currNode);
        std::unique_ptr<ast::ASTNode> parseConditional();
        std::unique_ptr<ast::ASTNode> parseWhileStatement();
        std::unique_ptr<ast::ASTNodeTypeDecl> parseType(bool disallowSpecialTypes = false);
        std::vector<std::shared_ptr<ast::ASTNode>> parseTemplateList();
        std::shared_ptr<ast::ASTNodeTypeDecl> parseUsingDeclaration();
        std::unique_ptr<ast::ASTNode> parsePadding();
        std::unique_ptr<ast::ASTNodeTypeDecl> parsePointerSizeType();
        std::unique_ptr<ast::ASTNode> parseMemberVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowPlacement);
        std::unique_ptr<ast::ASTNode> parseMemberArrayVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowPlacement);
        std::unique_ptr<ast::ASTNode> parseMemberPointerVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type);
        std::unique_ptr<ast::ASTNode> parseMemberPointerArrayVariable(const std::shared_ptr<ast::ASTNodeTypeDecl> &type);
        std::unique_ptr<ast::ASTNode> parseMember();
        std::shared_ptr<ast::ASTNodeTypeDecl> parseStruct();
        std::shared_ptr<ast::ASTNodeTypeDecl> parseUnion();
        std::shared_ptr<ast::ASTNodeTypeDecl> parseEnum();
        std::shared_ptr<ast::ASTNodeTypeDecl> parseBitfield();
        std::unique_ptr<ast::ASTNode> parseBitfieldEntry();
        void parseForwardDeclaration();
        std::unique_ptr<ast::ASTNode> parseVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type);
        std::unique_ptr<ast::ASTNode> parseArrayVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type);
        std::unique_ptr<ast::ASTNode> parsePointerVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type);
        std::unique_ptr<ast::ASTNode> parsePointerArrayVariablePlacement(const std::shared_ptr<ast::ASTNodeTypeDecl> &type);
        std::unique_ptr<ast::ASTNode> parsePlacement();
        std::vector<std::shared_ptr<ast::ASTNode>> parseNamespace();
        std::vector<std::shared_ptr<ast::ASTNode>> parseStatements();

        std::shared_ptr<ast::ASTNodeTypeDecl> addType(const std::string &name, std::unique_ptr<ast::ASTNode> &&node, std::optional<std::endian> endian = std::nullopt);

        std::vector<std::shared_ptr<ast::ASTNode>> parseTillToken(const Token &endToken) {
            std::vector<std::shared_ptr<ast::ASTNode>> program;

            while (!peek(endToken)) {
                for (auto &statement : parseStatements())
                    program.push_back(std::move(statement));
            }

            this->m_curr++;

            return program;
        }

        /* Token consuming */

        enum class Setting
        {
        };
        constexpr static auto Normal = static_cast<Setting>(0);
        constexpr static auto Not    = static_cast<Setting>(1);

        bool begin() {
            this->m_originalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        bool partBegin() {
            this->m_partOriginalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        void reset() {
            this->m_curr = this->m_originalPosition;
        }

        void partReset() {
            this->m_curr = this->m_partOriginalPosition;
        }

        bool resetIfFailed(bool value) {
            if (!value) reset();

            return value;
        }

        template<Setting S = Normal>
        bool sequenceImpl() {
            if constexpr (S == Normal)
                return true;
            else if constexpr (S == Not)
                return false;
            else
                hlp::unreachable();
        }

        template<Setting S = Normal>
        bool sequenceImpl(const Token &token, const auto &... args) {
            if constexpr (S == Normal) {
                if (!peek(token)) {
                    partReset();
                    return false;
                }

                this->m_curr++;

                if (!sequenceImpl<Normal>(args...)) {
                    partReset();
                    return false;
                }

                return true;
            } else if constexpr (S == Not) {
                if (!peek(token))
                    return true;

                this->m_curr++;

                if (!sequenceImpl<Normal>(args...))
                    return true;

                partReset();
                return false;
            } else
                hlp::unreachable();
        }

        template<Setting S = Normal>
        bool sequence(const Token &token, const auto &... args) {
            return partBegin() && sequenceImpl<S>(token, args...);
        }

        template<Setting S = Normal>
        bool oneOfImpl() {
            if constexpr (S == Normal)
                return false;
            else if constexpr (S == Not)
                return true;
            else
                hlp::unreachable();
        }

        template<Setting S = Normal>
        bool oneOfImpl(const Token &token, const auto &... args) {
            if constexpr (S == Normal)
                return sequenceImpl<Normal>(token) || oneOfImpl(args...);
            else if constexpr (S == Not)
                return sequenceImpl<Not>(token) && oneOfImpl(args...);
            else
                hlp::unreachable();
        }

        template<Setting S = Normal>
        bool oneOf(const Token &token, const auto &... args) {
            return partBegin() && oneOfImpl<S>(token, args...);
        }

        bool variantImpl(const Token &token1, const Token &token2) {
            if (!peek(token1)) {
                if (!peek(token2)) {
                    partReset();
                    return false;
                }
            }

            this->m_curr++;

            return true;
        }

        bool variant(const Token &token1, const Token &token2) {
            return partBegin() && variantImpl(token1, token2);
        }

        bool optionalImpl(const Token &token) {
            if (peek(token)) {
                this->m_matchedOptionals.push_back(this->m_curr);
                this->m_curr++;
            }

            return true;
        }

        bool optional(const Token &token) {
            return partBegin() && optionalImpl(token);
        }

        bool peek(const Token &token, i32 index = 0) {
            return this->m_curr[index].type == token.type && this->m_curr[index] == token.value;
        }
    };

}
