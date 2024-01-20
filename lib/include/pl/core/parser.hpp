#pragma once

#include <pl/helpers/types.hpp>
#include <pl/helpers/safe_iterator.hpp>

#include <pl/core/token.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/parser_manager.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_rvalue.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

#include <pl/core/errors/result.hpp>

#include <fmt/format.h>

#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace pl::core {

    template<typename T>
    class SafePointer : public T {
    public:
        using T::T;

        SafePointer(T &&t) : T(std::move(t)) { }

        using Contained = typename T::element_type;

        Contained* operator->() const {
            return this->get();
        }

        Contained* operator->() {
            return this->get();
        }

        Contained& operator*() const {
            return *this->get();
        }

        Contained& operator*() {
            return *this->get();
        }

        Contained* get() const {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return this->T::get();
        }

        operator T&() {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return *this;
        }

        operator T&() const {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return *this;
        }

        operator T&&() && {
            if (this->T::get() == nullptr)
                throw std::runtime_error("Pointer is null!");

            return *this;
        }
        
    };

    template<typename T>
    using safe_unique_ptr = SafePointer<std::unique_ptr<T>>;

    template<typename T>
    using safe_shared_ptr = SafePointer<std::shared_ptr<T>>;
    
    class Parser : err::ErrorCollector {
    public:
        using TokenIter = hlp::SafeIterator<std::vector<Token>::const_iterator>;

        explicit Parser() = default;
        ~Parser() override = default;

        hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>> parse(const std::vector<Token> &tokens);

        const auto &getTypes() { return this->m_types; }

        [[nodiscard]] const std::vector<std::string>& getGlobalDocComments() const {
            return this->m_globalDocComments;
        }

        void addNamespace(const std::vector<std::string>& path) {
            this->m_currNamespace.push_back(path);
        }

        void setParserManager(ParserManager* parserManager) {
            this->m_parserManager = parserManager;
        }

    private:
        TokenIter m_curr;
        TokenIter m_startToken, m_originalPosition, m_partOriginalPosition;

        std::vector<safe_shared_ptr<ast::ASTNodeTypeDecl>> m_currTemplateType;
        std::map<std::string, safe_shared_ptr<ast::ASTNodeTypeDecl>> m_types;

        std::vector<TokenIter> m_matchedOptionals;
        std::vector<std::vector<std::string>> m_currNamespace;

        std::vector<std::string> m_globalDocComments;
        i32 m_ignoreDocsCount = 0;
        std::vector<TokenIter> m_processedDocComments;

        ParserManager* m_parserManager = nullptr;

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
        safe_unique_ptr<T> create(Ts&&... ts) {
            auto temp = std::make_unique<T>(std::forward<Ts>(ts)...);
            temp->setLocation(this->m_curr[-1].location);
            return temp;
        }

        template<typename T, typename...Ts>
        safe_shared_ptr<T> createShared(Ts&&... ts) {
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

        std::vector<safe_unique_ptr<ast::ASTNode>> parseParameters();
        safe_unique_ptr<ast::ASTNode> parseFunctionCall();
        safe_unique_ptr<ast::ASTNode> parseStringLiteral();
        std::string parseNamespaceResolution();
        safe_unique_ptr<ast::ASTNode> parseScopeResolution();
        safe_unique_ptr<ast::ASTNode> parseRValue();
        safe_unique_ptr<ast::ASTNode> parseRValue(ast::ASTNodeRValue::Path &path);
        safe_unique_ptr<ast::ASTNode> parseFactor();
        safe_unique_ptr<ast::ASTNode> parseCastExpression();
        safe_unique_ptr<ast::ASTNode> parseUnaryExpression();
        safe_unique_ptr<ast::ASTNode> parseMultiplicativeExpression();
        safe_unique_ptr<ast::ASTNode> parseAdditiveExpression();
        safe_unique_ptr<ast::ASTNode> parseShiftExpression();
        safe_unique_ptr<ast::ASTNode> parseBinaryAndExpression();
        safe_unique_ptr<ast::ASTNode> parseBinaryXorExpression();
        safe_unique_ptr<ast::ASTNode> parseBinaryOrExpression(bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseBooleanAnd(bool inTemplate, bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseBooleanXor(bool inTemplate, bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseBooleanOr(bool inTemplate, bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseRelationExpression(bool inTemplate, bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseEqualityExpression(bool inTemplate, bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseTernaryConditional(bool inTemplate, bool inMatchRange);
        safe_unique_ptr<ast::ASTNode> parseMathematicalExpression(bool inTemplate = false, bool inMatchRange = false);

        safe_unique_ptr<ast::ASTNode> parseFunctionDefinition();
        safe_unique_ptr<ast::ASTNode> parseFunctionVariableDecl(bool constant = false);
        safe_unique_ptr<ast::ASTNode> parseFunctionStatement(bool needsSemicolon = true);
        safe_unique_ptr<ast::ASTNode> parseFunctionVariableAssignment(const std::string &lvalue);
        safe_unique_ptr<ast::ASTNode> parseFunctionVariableCompoundAssignment(const std::string &lvalue);
        safe_unique_ptr<ast::ASTNode> parseFunctionControlFlowStatement();
        std::vector<safe_unique_ptr<ast::ASTNode>> parseStatementBody(const std::function<safe_unique_ptr<ast::ASTNode>()> &memberParser);
        safe_unique_ptr<ast::ASTNode> parseFunctionWhileLoop();
        safe_unique_ptr<ast::ASTNode> parseFunctionForLoop();

        void parseAttribute(ast::Attributable *currNode);
        safe_unique_ptr<ast::ASTNode> parseConditional(const std::function<safe_unique_ptr<ast::ASTNode>()> &memberParser);
        std::pair<safe_unique_ptr<ast::ASTNode>, bool> parseCaseParameters(const std::vector<safe_unique_ptr<ast::ASTNode>> &condition);
        safe_unique_ptr<ast::ASTNode> parseMatchStatement(const std::function<safe_unique_ptr<ast::ASTNode>()> &memberParser);
        safe_unique_ptr<ast::ASTNode> parseTryCatchStatement(const std::function<safe_unique_ptr<ast::ASTNode>()> &memberParser);
        safe_unique_ptr<ast::ASTNode> parseWhileStatement();
        safe_unique_ptr<ast::ASTNodeTypeDecl> getCustomType(const std::string &baseTypeName);
        safe_unique_ptr<ast::ASTNodeTypeDecl> parseCustomType();
        void parseCustomTypeParameters(safe_unique_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNodeTypeDecl> parseType();
        std::vector<safe_shared_ptr<ast::ASTNode>> parseTemplateList();
        safe_shared_ptr<ast::ASTNodeTypeDecl> parseUsingDeclaration();
        safe_unique_ptr<ast::ASTNode> parsePadding();
        safe_unique_ptr<ast::ASTNode> parseMemberVariable(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowSection, bool constant, const std::string &identifier);
        safe_unique_ptr<ast::ASTNode> parseMemberArrayVariable(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type, bool allowSection, bool constant);
        safe_unique_ptr<ast::ASTNode> parseMemberPointerVariable(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNode> parseMemberPointerArrayVariable(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNode> parseMember();
        safe_shared_ptr<ast::ASTNodeTypeDecl> parseStruct();
        safe_shared_ptr<ast::ASTNodeTypeDecl> parseUnion();
        safe_shared_ptr<ast::ASTNodeTypeDecl> parseEnum();
        safe_shared_ptr<ast::ASTNodeTypeDecl> parseBitfield();
        safe_unique_ptr<ast::ASTNode> parseBitfieldEntry();
        void parseForwardDeclaration();
        safe_unique_ptr<ast::ASTNode> parseVariablePlacement(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNode> parseArrayVariablePlacement(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNode> parsePointerVariablePlacement(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNode> parsePointerArrayVariablePlacement(const safe_shared_ptr<ast::ASTNodeTypeDecl> &type);
        safe_unique_ptr<ast::ASTNode> parsePlacement();
        safe_unique_ptr<ast::ASTNode> parseImportStatement();
        std::vector<safe_shared_ptr<ast::ASTNode>> parseNamespace();
        std::vector<safe_shared_ptr<ast::ASTNode>> parseStatements();

        std::optional<i32> parseCompoundAssignment(const Token &token);

        std::optional<Token::DocComment> parseDocComment(bool global);

        safe_shared_ptr<ast::ASTNodeTypeDecl> addType(const std::string &name, safe_unique_ptr<ast::ASTNode> &&node, std::optional<std::endian> endian = std::nullopt);

        std::vector<safe_shared_ptr<ast::ASTNode>> parseTillToken(const Token &endToken) {
            std::vector<safe_shared_ptr<ast::ASTNode>> program;

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

        enum class Setting {
            Normal = 0,
            Not = 1
        };
        using enum Parser::Setting;

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

        template<Setting S = Normal>
        bool sequenceImpl() {
            if constexpr (S == Normal)
                return true;
            else if constexpr (S == Not)
                return false;
            else
                std::unreachable();
        }

        template<Setting S = Normal>
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

                this->next();
                partReset();
                return false;
            } else
                std::unreachable();
        }

        template<Setting S = Normal>
        bool sequenceImpl(const auto &... args) {
            return (matchOne<S>(args) && ...);
        }

        template<Setting S = Normal>
        bool sequence(const Token &token, const auto &... args) {
            partBegin();
            return sequenceImpl<S>(token, args...);
        }

        template<Setting S = Normal>
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

        template<Setting S = Normal>
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
    };

}
