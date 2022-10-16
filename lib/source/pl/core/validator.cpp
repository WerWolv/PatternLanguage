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

#include <unordered_set>
#include <string>

namespace pl::core {

    bool Validator::validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        std::unordered_set<std::string> identifiers;

        this->m_recursionDepth++;
        try {

            for (const auto &node : ast) {
                if (node == nullptr)
                    err::V0001.throwError("Null-Pointer found in AST.", "This is a parser bug. Please report it on GitHub.");

                this->m_lastNode = node.get();

                // Variables named "$padding$" are paddings and can appear multiple times per type definition
                identifiers.erase("$padding$");

                if (this->m_recursionDepth > this->m_maxRecursionDepth)
                    err::V0003.throwError(fmt::format("Type recursion depth exceeded set limit of '{}'.", this->m_maxRecursionDepth), "If this is intended, try increasing the limit using '#pragma eval_depth <new_limit>'.");

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
                } else if (auto functionNode = dynamic_cast<ast::ASTNodeFunctionDefinition *>(node.get()); functionNode != nullptr) {
                    if (!identifiers.insert(functionNode->getName()).second)
                        err::V0002.throwError(fmt::format("Redefinition of identifier '{0}'", functionNode->getName()));

                    std::unordered_set<std::string> parameterIdentifiers;
                    for (const auto &[name, type] : functionNode->getParams()) {
                        if (!parameterIdentifiers.insert(name).second)
                            err::V0002.throwError(fmt::format("Redefinition of function parameter '{0}'", name));
                    }
                }
            }

        } catch (err::ValidatorError::Exception &e) {
            if (this->m_lastNode != nullptr) {
                auto line = this->m_lastNode->getLine();
                auto column = this->m_lastNode->getColumn();

                this->m_error = err::PatternLanguageError(e.format(sourceCode, line, column), line, column);
            }
            else
                this->m_error = err::PatternLanguageError(e.format(sourceCode, 1, 1), 1, 1);

            this->m_recursionDepth = 0;

            return false;
        }

        this->m_recursionDepth--;

        return true;
    }
}