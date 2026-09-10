#pragma once

#include <pl/core/ast/visitors/ast_validator.hpp>
#include <stack>

namespace pl::core {
    class ScopeValidator final : public ASTValidator {

        struct Scope {
            std::unordered_set<std::string> m_names;

            [[nodiscard]] bool newIdentifier(const std::string& name);

            void mergeWith(Scope& from);
        };

        std::stack<Scope> m_scopes;

        u32 m_curNonFuncScopes = 0;

    public:
        ScopeValidator();

    private:
        void reset() override;

        void pushScope();
        void pushScope(Scope&& scope);
        void popScope();
        Scope &currentScope();

        [[nodiscard]] bool isFunctionScope() const;

        [[nodiscard]] bool newIdentifier(const std::string& name);


        void visit(ast::ASTNodeVariableDecl& node) override;
        void visit(ast::ASTNodeArrayVariableDecl& node) override;
        void visit(ast::ASTNodePointerVariableDecl& node) override;
        void visit(ast::ASTNodeBitfieldField& node) override;
        void visit(ast::ASTNodeMultiVariableDecl& node) override;
        void visit(ast::ASTNodeTypeDecl& node) override;
        void visit(ast::ASTNodeStruct& node) override;
        void visit(ast::ASTNodeUnion& node) override;
        void visit(ast::ASTNodeBitfield& node) override;
        void visit(ast::ASTNodeBitfieldArrayVariableDecl& node) override;
        void visit(ast::ASTNodeEnum& node) override;
        void visit(ast::ASTNodeConditionalStatement& node) override;
        void visit(ast::ASTNodeFunctionDefinition& node) override;
        void visit(ast::ASTNodeCompoundStatement& node) override;
        void visit(ast::ASTNodeWhileStatement& node) override;
        void visit(ast::ASTNodeMatchStatement& node) override;
        void visit(ast::ASTNodeTryCatchStatement& node) override;
    };
}