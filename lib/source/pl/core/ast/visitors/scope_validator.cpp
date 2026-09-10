
#include <pl/core/ast/visitors/scope_validator.hpp>

#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_bitfield_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>
#include <pl/core/ast/ast_node_compound_statement.hpp>
#include <pl/core/ast/ast_node_conditional_statement.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_match_statement.hpp>
#include <pl/core/ast/ast_node_multi_variable_decl.hpp>
#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_try_catch_statement.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>

namespace pl::core {
    bool ScopeValidator::Scope::newIdentifier(const std::string &name) {
        // Variables named "$padding$" are paddings and can appear multiple times per type definition
        // Variables that don't have a name are anonymous and can appear multiple times per type definition
        // Let's just ignore them altogether.
        if (name == "$padding$" || name == "") {
            return true;
        }

        auto [_, inserted] = m_names.insert(name);
        return inserted;
    }

    void ScopeValidator::Scope::mergeWith(ScopeValidator::Scope &from) {
        m_names.merge(from.m_names);
    }

    ScopeValidator::ScopeValidator() {
        reset();
    }

    void ScopeValidator::reset() {
        m_scopes = std::stack<Scope>();
        // Global scope
        pushScope();
        m_curNonFuncScopes = 0;
    }

    void ScopeValidator::pushScope() {
        m_scopes.emplace();
    }
    void ScopeValidator::pushScope(Scope &&scope) {
        m_scopes.push(std::move(scope));
    }
    void ScopeValidator::popScope() {
        m_scopes.pop();
    }

    ScopeValidator::Scope &ScopeValidator::currentScope() {
        return m_scopes.top();
    }

    [[nodiscard]] bool ScopeValidator::isFunctionScope() const {
        return m_curNonFuncScopes == 0;
    }

    [[nodiscard]] bool ScopeValidator::newIdentifier(const std::string &name) {
        return currentScope().newIdentifier(name);
    }

    void ScopeValidator::visit(ast::ASTNodeVariableDecl &node) {
        const auto& var_name = node.getName();
        if (!newIdentifier(var_name)) {
            error("Redeclaration of identifier '{}'", var_name);
        }
    }

    void ScopeValidator::visit(ast::ASTNodeArrayVariableDecl &node) {
        const auto& var_name = node.getName();
        if (!newIdentifier(var_name)) {
            error("Redeclaration of identifier '{}'", var_name);
        }
    }

    void ScopeValidator::visit(ast::ASTNodePointerVariableDecl &node) {
        const auto& var_name = node.getName();
        if (!newIdentifier(var_name)) {
            error("Redeclaration of identifier '{}'", var_name);
        }
    }

    void ScopeValidator::visit(ast::ASTNodeBitfieldField &node) {
        const auto& field_name = node.getName();
        if (!newIdentifier(field_name)) {
            error("Redefinition of identifier '{}'", field_name);
        }
    }

    void ScopeValidator::visit(ast::ASTNodeMultiVariableDecl &node) {
        visitTypeErasedNodes(node.getVariables());
    }

    void ScopeValidator::visit(ast::ASTNodeTypeDecl &node) {
        if (node.isForwardDeclared()) {
            return;
        }
        // TODO: lint this
        Scope typeParameterIdentifiers;
        Scope nonTypeParameterIdentifiers;

        for (const auto& param: node.getTemplateParameters()) {
            const auto& name = param->getName().get();
            auto isType = param->isType();
            if (isType && !typeParameterIdentifiers.newIdentifier(name)) {
                error("Redefinition of type template parameter '{}'", name);
                return;
            }
            if (!isType && !nonTypeParameterIdentifiers.newIdentifier(name)) {
                error("Redefinition of non-type template parameter '{}'", name);
                return;
            }
        }

        pushScope(std::move(nonTypeParameterIdentifiers));
        if (node.isValid()) {
            visitTypeErased(node.getType());
        }
        popScope();
    }

    void ScopeValidator::visit(ast::ASTNodeStruct &node) {
        ++m_curNonFuncScopes;
        visitTypeErasedNodes(node.getMembers());
        --m_curNonFuncScopes;
    }

    void ScopeValidator::visit(ast::ASTNodeUnion &node) {
        ++m_curNonFuncScopes;
        visitTypeErasedNodes(node.getMembers());
        --m_curNonFuncScopes;
    }

    void ScopeValidator::visit(ast::ASTNodeBitfield &node) {
        ++m_curNonFuncScopes;
        visitTypeErasedNodes(node.getEntries());
        --m_curNonFuncScopes;
    }

    void ScopeValidator::visit(ast::ASTNodeBitfieldArrayVariableDecl &node) {
        const auto& field_name = node.getName();
        if (!newIdentifier(field_name)) {
            error("Redefinition of identifier '{}'", field_name);
        }
    }

    void ScopeValidator::visit(ast::ASTNodeEnum &node) {
        // TODO: enums are broken
        pushScope();
        for (const auto &[name, _] : node.getEntries()) {
            if (!newIdentifier(name)) {
                error("Redeclaration of enum entry '{}'", name);
                return;
            }
        }
        popScope();
    }

    void ScopeValidator::visit(ast::ASTNodeConditionalStatement &node) {
        const auto initialScope = currentScope();

        // The branches are allowed to introduce the same identifiers
        if (isFunctionScope()) {
            // ... which won't be accessible after 'if' ends.

            visitTypeErasedNodes(node.getTrueBody());
            currentScope() = initialScope;

            visitTypeErasedNodes(node.getFalseBody());
            currentScope() = initialScope;
            return;
        }
        // ... which will be accessible after 'if' ends => assume both paths are taken.

        visitTypeErasedNodes(node.getTrueBody());
        auto afterIfScope = currentScope();

        currentScope() = initialScope;
        visitTypeErasedNodes(node.getFalseBody());

        currentScope().mergeWith(afterIfScope);
    }

    void ScopeValidator::visit(ast::ASTNodeFunctionDefinition &node) {
        if (!newIdentifier(node.getName())) {
            error("Redefinition of identifier '{}'", node.getName());
        }

        pushScope();
        for (const auto &[name, _] : node.getParams()) {
            if (!newIdentifier(name)) {
                error("Redefinition of function parameter '{}'", name);
                return;
            }
        }
        visitTypeErasedNodes(node.getBody());
        popScope();
    }

    void ScopeValidator::visit(ast::ASTNodeCompoundStatement &node) {
        visitTypeErasedNodes(node.getStatements());
    }

    void ScopeValidator::visit(ast::ASTNodeWhileStatement &node) {
        const auto initialScope = currentScope();
        visitTypeErasedNodes(node.getBody());
        currentScope() = initialScope;
    }

    void ScopeValidator::visit(ast::ASTNodeMatchStatement &node) {
        // The branches are allowed to introduce the same identifiers
        const auto initialScope = currentScope();
        if (isFunctionScope()) {
            // ... which won't be accessible after 'match' ends.

            for (const auto& match_case: node.getNonDefaultCases()) {
                visitTypeErasedNodes(match_case.body);
                currentScope() = initialScope;
            }

            if (const auto& default_case = node.getDefaultCase(); default_case) {
                visitTypeErasedNodes((*default_case).body);
                currentScope() = initialScope;
            }
            return;
        }

        // ... which will be accessible after 'match' ends => like with the if statement,
        // assume all match paths are taken and accumulate the ids.

        auto resultingScope = currentScope();

        for (const auto& match_case: node.getNonDefaultCases()) {
            visitTypeErasedNodes(match_case.body);
            resultingScope.mergeWith(currentScope());
            currentScope() = initialScope;
        }

        if (const auto& default_case = node.getDefaultCase(); default_case) {
            visitTypeErasedNodes((*default_case).body);
            resultingScope.mergeWith(currentScope());
        }

        currentScope() = std::move(resultingScope);
    }

    void ScopeValidator::visit(ast::ASTNodeTryCatchStatement &node) {
        const auto initialScope = currentScope();

        // The branches are allowed to introduce the same identifiers
        if (isFunctionScope()) {
            // ... which won't be accessible after 'try' ends.

            visitTypeErasedNodes(node.getTryBody());
            currentScope() = initialScope;

            visitTypeErasedNodes(node.getCatchBody());
            currentScope() = initialScope;
            return;
        }
        // ... which will be accessible after 'try' ends => assume both paths are taken.

        visitTypeErasedNodes(node.getTryBody());
        auto afterTryScope = currentScope();

        currentScope() = initialScope;
        visitTypeErasedNodes(node.getCatchBody());

        currentScope().mergeWith(afterTryScope);
    }


}