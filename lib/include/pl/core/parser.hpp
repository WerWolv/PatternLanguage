#pragma once

#include <pl/helpers/types.hpp>
#include <pl/helpers/safe_iterator.hpp>
#include <pl/helpers/safe_pointer.hpp>

#include <pl/core/token.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/parser_manager.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_rvalue.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

#include <pl/core/errors/result.hpp>

#include <fmt/core.h>

#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace pl::core {
    
    class Parser : err::ErrorCollector {

        class UnrecoverableParserException : public std::exception {};

    public:
        using TokenIter = hlp::SafeIterator<std::vector<Token>::iterator>;

        explicit Parser() = default;
        ~Parser() override = default;

        hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> parse(std::vector<Token> &tokens);

        const auto &getTypes() { return this->m_types; }

        [[nodiscard]] const std::vector<std::string>& getGlobalDocComments() const {
            return this->m_globalDocComments;
        }

        void setParserManager(ParserManager* parserManager) {
            this->m_parserManager = parserManager;
        }

    private:
        TokenIter m_curr;
        TokenIter m_startToken, m_originalPosition, m_partOriginalPosition;

        std::vector<hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>> m_currTemplateType;
        std::map<std::string, hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>> m_types;

        std::vector<TokenIter> m_matchedOptionals;
        std::vector<std::vector<std::string>> m_currNamespace;

        std::vector<std::string> m_globalDocComments;
        i32 m_ignoreDocsCount = 0;
        std::vector<TokenIter> m_processedDocComments;

        ParserManager* m_parserManager = nullptr;

        std::vector<std::string> m_aliasNamespace;
        std::string m_aliasNamespaceString;
        std::string m_autoNamespace;

        Location location() override;
        // error helpers
        void errorHere(const std::string &message);
        template <typename... Args>
        void errorHere(const fmt::format_string<Args...> &fmt, Args&&... args) {
            errorHere(fmt::format(fmt, std::forward<Args>(args)...));
        }
        void errorDescHere(const std::string &message, const std::string &description);
        template <typename... Args>
        void errorDescHere(const fmt::format_string<Args...> &fmt, const std::string &description, Args&&... args) {
            errorDescHere(fmt::format(fmt, std::forward<Args>(args)...), description);
        }

        void addGlobalDocComment(const std::string &comment) {
            this->m_globalDocComments.push_back(comment);
        }

        template<typename T, typename...Ts>
        hlp::safe_unique_ptr<T> create(Ts&&... ts) {
            auto temp = std::make_unique<T>(std::forward<Ts>(ts)...);
            temp->setLocation(this->m_curr[-1].location);
            return temp;
        }

        template<typename T, typename...Ts>
        hlp::safe_unique_ptr<T> createWithLocation(const Location &loc, Ts&&... ts) {
            auto temp = std::make_unique<T>(std::forward<Ts>(ts)...);
            temp->setLocation(loc);
            return temp;
        }

        template<typename T, typename...Ts>
        hlp::safe_shared_ptr<T> createShared(Ts&&... ts) {
            auto temp = std::make_shared<T>(std::forward<Ts>(ts)...);
            temp->setLocation(this->m_curr[-1].location);
            return temp;
        }

        template<typename T>
        const T &getValue(const i32 index) {
            auto &token = this->m_curr[index];
            auto value = std::get_if<T>(&token.value);

            if (value == nullptr) {
                std::visit([&](auto &&) {
                    errorDesc("Expected {}, got {}.", "This is a serious parsing bug. Please open an issue on GitHub!", typeid(T).name(), typeid(value).name());
                    throw UnrecoverableParserException();
                }, token.value);
            }

            return *value;
        }

        [[nodiscard]] std::string getFormattedToken(const i32 index) const {
            return fmt::format("{} ({})", this->m_curr[index].getFormattedType(), this->m_curr[index].getFormattedValue());
        }

        [[nodiscard]] std::vector<std::string> getNamespacePrefixedNames(const std::string &name) const {
            std::vector<std::string> result;

            result.push_back(name);

            std::string namespacePrefix;
            for (const auto &part : this->m_currNamespace.back()) {
                namespacePrefix += fmt::format("{}::", part);
                result.push_back(fmt::format("{}{}", namespacePrefix, name));
            }

            return result;
        }

        void next() {
            ++this->m_curr;
        }

        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> parseParameters();
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionCall();
        hlp::safe_unique_ptr<ast::ASTNode> parseStringLiteral();
        std::string parseNamespaceResolution();
        hlp::safe_unique_ptr<ast::ASTNode> parseScopeResolution();
        hlp::safe_unique_ptr<ast::ASTNode> parseRValue();
        hlp::safe_unique_ptr<ast::ASTNode> parseRValue(ast::ASTNodeRValue::Path &path);
        hlp::safe_unique_ptr<ast::ASTNode> parseRValueAssignment();
        hlp::safe_unique_ptr<ast::ASTNode> parseUserDefinedLiteral(hlp::safe_unique_ptr<ast::ASTNode> &&literal);
        hlp::safe_unique_ptr<ast::ASTNode> parseFactor();
        hlp::safe_unique_ptr<ast::ASTNode> parseCastExpression();
        hlp::safe_unique_ptr<ast::ASTNode> parseReinterpretExpression();
        hlp::safe_unique_ptr<ast::ASTNode> parseUnaryExpression();
        hlp::safe_unique_ptr<ast::ASTNode> parseMultiplicativeExpression();
        hlp::safe_unique_ptr<ast::ASTNode> parseAdditiveExpression();
        hlp::safe_unique_ptr<ast::ASTNode> parseShiftExpression(bool inTemplate);
        hlp::safe_unique_ptr<ast::ASTNode> parseBinaryAndExpression(bool inTemplate);
        hlp::safe_unique_ptr<ast::ASTNode> parseBinaryXorExpression(bool inTemplate);
        hlp::safe_unique_ptr<ast::ASTNode> parseBinaryOrExpression(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseBooleanAnd(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseBooleanXor(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseBooleanOr(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseRelationExpression(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseEqualityExpression(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseTernaryConditional(bool inTemplate, bool inMatchRange);
        hlp::safe_unique_ptr<ast::ASTNode> parseMathematicalExpression(bool inTemplate = false, bool inMatchRange = false);
        hlp::safe_unique_ptr<ast::ASTNode> parseArrayInitExpression(std::string identifier);


        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionDefinition();
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionVariableDecl(bool constant = false);
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionStatement(bool needsSemicolon = true);
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionVariableAssignment(const std::string &lvalue);
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionVariableCompoundAssignment(const std::string &lvalue);
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionControlFlowStatement();
        std::vector<hlp::safe_unique_ptr<ast::ASTNode>> parseStatementBody(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser);
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionWhileLoop();
        hlp::safe_unique_ptr<ast::ASTNode> parseFunctionForLoop();

        void parseAttribute(ast::Attributable *currNode);
        hlp::safe_unique_ptr<ast::ASTNode> parseConditional(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser);
        std::pair<hlp::safe_unique_ptr<ast::ASTNode>, bool> parseCaseParameters(const std::vector<hlp::safe_unique_ptr<ast::ASTNode>> &condition);
        hlp::safe_unique_ptr<ast::ASTNode> parseMatchStatement(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser);
        hlp::safe_unique_ptr<ast::ASTNode> parseTryCatchStatement(const std::function<hlp::safe_unique_ptr<ast::ASTNode>()> &memberParser);
        hlp::safe_unique_ptr<ast::ASTNode> parseWhileStatement();
        hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> getCustomType(const std::string &baseTypeName);
        hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> parseCustomType();
        void parseCustomTypeParameters(hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNodeTypeDecl> parseType();
        std::vector<hlp::safe_shared_ptr<ast::ASTNode>> parseTemplateList();
        hlp::safe_shared_ptr<ast::ASTNode> parseImportStatement();
        hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> parseUsingDeclaration();
        hlp::safe_unique_ptr<ast::ASTNode> parsePadding();
        hlp::safe_unique_ptr<ast::ASTNode> parseMemberVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type, bool constant, const std::string &identifier);
        hlp::safe_unique_ptr<ast::ASTNode> parseMemberArrayVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type, bool constant);
        hlp::safe_unique_ptr<ast::ASTNode> parseMemberPointerVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNode> parseMemberPointerArrayVariable(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNode> parseMember();
        hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> parseStruct();
        hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> parseUnion();
        hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> parseEnum();
        hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> parseBitfield();
        hlp::safe_unique_ptr<ast::ASTNode> parseBitfieldEntry();
        void parseForwardDeclaration();
        hlp::safe_unique_ptr<ast::ASTNode> parseVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNode> parseArrayVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNode> parsePointerVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNode> parsePointerArrayVariablePlacement(const hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        hlp::safe_unique_ptr<ast::ASTNode> parsePlacement();
        std::vector<hlp::safe_shared_ptr<ast::ASTNode>> parseNamespace();
        std::vector<hlp::safe_shared_ptr<ast::ASTNode>> parseStatements();

        std::optional<i32> parseCompoundAssignment(const Token &token);

        std::optional<Token::DocComment> parseDocComment(bool global);

        hlp::safe_shared_ptr<ast::ASTNodeTypeDecl> addType(const std::string &name, hlp::safe_unique_ptr<ast::ASTNode> &&node, std::optional<std::endian> endian = std::nullopt);

        void includeGuard();

        std::vector<hlp::safe_shared_ptr<ast::ASTNode>> parseTillToken(const Token &endToken) {
            std::vector<hlp::safe_shared_ptr<ast::ASTNode>> program;

            while (!peek(endToken)) {
                for (auto &statement : parseStatements())
                    program.push_back(std::move(statement));

                if (hasErrors())
                    break;
            }

            this->next();

            return program;
        }

        /* Token consuming */

        constexpr static u32 Normal = 0;
        constexpr static u32 Not    = 1;

        bool begin() {
            this->m_originalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        void partBegin() {
            this->m_partOriginalPosition = this->m_curr;
            this->m_matchedOptionals.clear();
        }

        void reset() {
            this->m_curr = this->m_originalPosition;
        }

        void partReset() {
            this->m_curr = this->m_partOriginalPosition;
        }

        bool resetIfFailed(const bool value) {
            if (!value) reset();

            return value;
        }

        template<auto S = Normal>
        bool sequenceImpl() {
            if constexpr (S == Normal)
                return true;
            else if constexpr (S == Not)
                return false;
            else
                std::unreachable();
        }

        template<auto S = Normal>
        bool matchOne(const Token &token) {
            if constexpr (S == Normal) {
                if (!peek(token)) {
                    partReset();
                    return false;
                }

                this->next();
                return true;
            } else if constexpr (S == Not) {
                if (!peek(token))
                    return true;

                partReset();
                return false;
            } else
                std::unreachable();
        }

        template<auto S = Normal>
        bool sequenceImpl(const auto &... args) {
            return (matchOne<S>(args) && ...);
        }

        template<auto S = Normal>
        bool sequence(const Token &token, const auto &... args) {
            partBegin();
            return sequenceImpl<S>(token, args...);
        }

        template<auto S = Normal>
        bool oneOfImpl(const auto &... args) {
            if constexpr (S == Normal) {
                if (!(... || peek(args))) {
                    partReset();
                    return false;
                }

                this->next();

                return true;
            } else if constexpr (S == Not) {
                if (!(... && peek(args)))
                    return true;

                this->next();

                partReset();
                return false;
            } else
                std::unreachable();
        }

        template<auto S = Normal>
        bool oneOf(const Token &token, const auto &... args) {
            partBegin();
            return oneOfImpl<S>(token, args...);
        }

        bool variantImpl(const Token &token1, const Token &token2) {
            if (!peek(token1)) {
                if (!peek(token2)) {
                    partReset();
                    return false;
                }
            }

            this->next();

            return true;
        }

        bool variant(const Token &token1, const Token &token2) {
            partBegin();
            return variantImpl(token1, token2);
        }

        bool optionalImpl(const Token &token) {
            if (peek(token)) {
                this->m_matchedOptionals.push_back(this->m_curr);
                this->next();
            }

            return true;
        }

        bool optional(const Token &token) {
            partBegin();
            return optionalImpl(token);
        }

        bool peek(const Token &token, const i32 index = 0) {
            if (index >= 0) {
                while (this->m_curr->type == Token::Type::DocComment) {
                    if (auto docComment = parseDocComment(true); docComment.has_value())
                        this->addGlobalDocComment(docComment->comment);
                    this->next();
                }
            } else {
                while (this->m_curr->type == Token::Type::DocComment) {
                    if (auto docComment = parseDocComment(true); docComment.has_value())
                        this->addGlobalDocComment(docComment->comment);
                    --this->m_curr;
                }
            }

            return this->m_curr[index].type == token.type && this->m_curr[index] == token.value;
        }

        friend class ParserManager;
    };

}
