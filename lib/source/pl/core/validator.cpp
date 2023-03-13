#include <pl/core/validator.hpp>

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_array_variable_decl.hpp>
#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>
#include <pl/core/ast/ast_node_multi_variable_decl.hpp>
#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>
#include <pl/core/ast/ast_node_conditional_statement.hpp>

#include <unordered_set>
#include <string>

namespace pl::core {

    bool Validator::validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast, bool newScope, bool firstRun) {
        if (firstRun) {
            this->m_recursionDepth = 0;
            this->m_validatedNodes.clear();
            this->m_identifiers.clear();
        }

        newScope = newScope || this->m_identifiers.empty();
        if (newScope)
            this->m_identifiers.emplace_back();

        auto &identifiers = this->m_identifiers.back();

        this->m_recursionDepth++;
        ON_SCOPE_EXIT {
            this->m_recursionDepth--;

            if (newScope)
                this->m_identifiers.pop_back();
        };

        try {

            for (const auto &node : ast) {
                if (this->m_validatedNodes.contains(node.get()))
                    continue;

                if (node == nullptr)
                    err::V0001.throwError("Null-Pointer found in AST.", "This is a parser bug. Please report it on GitHub.");

                this->m_lastNode = node.get();

                // Variables named "$padding$" are paddings and can appear multiple times per type definition
                identifiers.erase("$padding$");
                // Variables that don't have a name are anonymous and can appear multiple times per type definition
                identifiers.erase("");

                if (this->m_recursionDepth > this->m_maxRecursionDepth)
                    return true;

                if (auto variableDeclNode = dynamic_cast<ast::ASTNodeVariableDecl *>(node.get()); variableDeclNode != nullptr) {
                    if (!identifiers.insert(variableDeclNode->getName()).second)
                        err::V0002.throwError(fmt::format("Redefinition of identifier '{0}'", variableDeclNode->getName()));

                    if (!this->validate(sourceCode, { variableDeclNode->getType() }))
                        return false;
                } else if (auto arrayVariableDeclNode = dynamic_cast<ast::ASTNodeArrayVariableDecl *>(node.get()); arrayVariableDeclNode != nullptr) {
                    if (!identifiers.insert(arrayVariableDeclNode->getName()).second)
                        err::V0002.throwError(fmt::format("Redefinition of identifier '{0}'", arrayVariableDeclNode->getName()));

                    if (!this->validate(sourceCode, { arrayVariableDeclNode->getType() }))
                        return false;
                } else if (auto pointerVariableDecl = dynamic_cast<ast::ASTNodePointerVariableDecl *>(node.get()); pointerVariableDecl != nullptr) {
                    if (!identifiers.insert(pointerVariableDecl->getName()).second)
                        err::V0002.throwError(fmt::format("Redefinition of identifier '{0}'", pointerVariableDecl->getName()));

                    if (!this->validate(sourceCode, { pointerVariableDecl->getType() }))
                        return false;
                } else if (auto bitfieldFieldDecl = dynamic_cast<ast::ASTNodeBitfieldField *>(node.get()); bitfieldFieldDecl != nullptr) {
                    if (!identifiers.insert(bitfieldFieldDecl->getName()).second)
                        err::V0002.throwError(fmt::format("Redefinition of identifier '{0}'", bitfieldFieldDecl->getName()));
                } else if (auto multiVariableDecl = dynamic_cast<ast::ASTNodeMultiVariableDecl *>(node.get()); multiVariableDecl != nullptr) {
                    if (!this->validate(sourceCode, multiVariableDecl->getVariables()))
                        return false;
                } else if (auto typeDeclNode = dynamic_cast<ast::ASTNodeTypeDecl *>(node.get()); typeDeclNode != nullptr) {
                    if (!typeDeclNode->isForwardDeclared())
                        if (!this->validate(sourceCode, { typeDeclNode->getType() }))
                            return false;
                } else if (auto structNode = dynamic_cast<ast::ASTNodeStruct *>(node.get()); structNode != nullptr) {
                    if (!this->validate(sourceCode, structNode->getMembers()))
                        return false;
                } else if (auto unionNode = dynamic_cast<ast::ASTNodeUnion *>(node.get()); unionNode != nullptr) {
                    if (!this->validate(sourceCode, unionNode->getMembers()))
                        return false;
                } else if (auto bitfieldNode = dynamic_cast<ast::ASTNodeBitfield *>(node.get()); bitfieldNode != nullptr) {
                    if (!this->validate(sourceCode, bitfieldNode->getEntries()))
                        return false;
                } else if (auto enumNode = dynamic_cast<ast::ASTNodeEnum *>(node.get()); enumNode != nullptr) {
                    std::unordered_set<std::string> enumIdentifiers;
                    for (auto &[name, value] : enumNode->getEntries()) {
                        if (!enumIdentifiers.insert(name).second)
                            err::V0002.throwError(fmt::format("Redefinition of enum entry '{0}'", name));
                    }
                } else if (auto conditionalNode = dynamic_cast<ast::ASTNodeConditionalStatement *>(node.get()); conditionalNode != nullptr) {
                    auto prevIdentifiers = identifiers;
                    if (!this->validate(sourceCode, conditionalNode->getTrueBody(), false))
                        return false;
                    identifiers = prevIdentifiers;
                    if (!this->validate(sourceCode, conditionalNode->getFalseBody(), false))
                        return false;
                    identifiers = prevIdentifiers;
                } else if (auto functionNode = dynamic_cast<ast::ASTNodeFunctionDefinition *>(node.get()); functionNode != nullptr) {
                    if (!identifiers.insert(functionNode->getName()).second)
                        err::V0002.throwError(fmt::format("Redefinition of identifier '{0}'", functionNode->getName()));

                    std::unordered_set<std::string> parameterIdentifiers;
                    for (const auto &[name, type] : functionNode->getParams()) {
                        if (!parameterIdentifiers.insert(name).second)
                            err::V0002.throwError(fmt::format("Redefinition of function parameter '{0}'", name));
                    }
                }

                this->m_validatedNodes.insert(node.get());
            }

        } catch (err::ValidatorError::Exception &e) {
            if (this->m_lastNode != nullptr) {
                auto line = this->m_lastNode->getLine();
                auto column = this->m_lastNode->getColumn();

                this->m_error = err::PatternLanguageError(e.format(sourceCode, line, column), line, column);
            }
            else
                this->m_error = err::PatternLanguageError(e.format(sourceCode, 1, 1), 1, 1);

            return false;
        }

        return true;
    }

    bool Validator::validate(const std::string &sourceCode, const std::vector<std::unique_ptr<ast::ASTNode>> &ast, bool newScope, bool firstRun) {
        std::vector<std::shared_ptr<ast::ASTNode>> sharedAst;
        sharedAst.reserve(ast.size());
        for (const auto &node : ast)
            sharedAst.emplace_back(node->clone());

        return this->validate(sourceCode, sharedAst, newScope, firstRun);
    }
}