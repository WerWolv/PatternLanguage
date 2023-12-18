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

    Validator::Result Validator::validate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        this->m_recursionDepth = 0;
        this->m_validatedNodes.clear();
        this->m_identifiers.clear();

        validateNodes(ast);

        if(this->hasErrors()) {
            return Result::err(this->collectErrors());
        }
        return { };
    }

    bool Validator::validateNodes(const std::vector<std::shared_ptr<ast::ASTNode>>& nodes, const bool newScope) {
        if (newScope || this->m_identifiers.empty())
            this->m_identifiers.emplace_back();

        auto &identifiers = this->m_identifiers.back();

        this->m_recursionDepth++;
        ON_SCOPE_EXIT {
            this->m_recursionDepth--;

            if (newScope)
                this->m_identifiers.pop_back();
        };

        bool success = true;
        for (const auto &node : nodes) {
            if (this->m_validatedNodes.contains(node.get()))
                continue;

            if (node == nullptr) {
                errorDesc("Null-Pointer found in AST.", "This is a parser bug. Please report it on GitHub.");
                continue;
            }

            this->m_lastNode = node.get();

            // Variables named "$padding$" are paddings and can appear multiple times per type definition
            identifiers.erase("$padding$");
            // Variables that don't have a name are anonymous and can appear multiple times per type definition
            identifiers.erase("");

            if (this->m_recursionDepth > this->m_maxRecursionDepth)
                break;

            if(!validateNode(node.get(), identifiers))
                success = false;
        }
        return success;
    }

    bool Validator::validateNodes(const std::vector<std::unique_ptr<ast::ASTNode>>& nodes, bool newScope) {
        // move into vector of shared_ptr
        std::vector<std::shared_ptr<ast::ASTNode>> sharedNodes;
        sharedNodes.reserve(nodes.size());
        for (const auto& node : nodes) {
            sharedNodes.emplace_back(node->clone());
        }

        return validateNodes(sharedNodes, newScope);
    }


    bool Validator::validateNode(const std::shared_ptr<ast::ASTNode>& node, const bool newScope) {
        return this->validateNodes({ node }, newScope);
    }


    bool Validator::validateNode(ast::ASTNode* node, std::unordered_set<std::string>& identifiers) {
        if (const auto variableDeclNode = dynamic_cast<ast::ASTNodeVariableDecl *>(node); variableDeclNode != nullptr) {
            if (!identifiers.insert(variableDeclNode->getName()).second) {
                error("Redeclaration of identifier '{}'", variableDeclNode->getName());
                return false;
            }

            if (!this->validateNode(variableDeclNode->getType()))
                return false;
        } else if (const auto arrayVariableDeclNode = dynamic_cast<ast::ASTNodeArrayVariableDecl *>(node); arrayVariableDeclNode != nullptr) {
            if (!identifiers.insert(arrayVariableDeclNode->getName()).second) {
                error("Redeclaration of identifier '{}'", arrayVariableDeclNode->getName());
                return false;
            }

            if (!this->validateNode(arrayVariableDeclNode->getType()))
                return false;
        } else if (const auto pointerVariableDecl = dynamic_cast<ast::ASTNodePointerVariableDecl *>(node); pointerVariableDecl != nullptr) {
            if (!identifiers.insert(pointerVariableDecl->getName()).second) {
                error("Redeclaration of identifier '{}'", pointerVariableDecl->getName());
                return false;
            }

            if (!this->validateNode(pointerVariableDecl->getType()))
                return false;
        } else if (const auto bitfieldFieldDecl = dynamic_cast<ast::ASTNodeBitfieldField *>(node); bitfieldFieldDecl != nullptr) {
            if (!identifiers.insert(bitfieldFieldDecl->getName()).second) {
                error("Redefinition of identifier '{}'", bitfieldFieldDecl->getName());
                return false;
            }
        } else if (const auto multiVariableDecl = dynamic_cast<ast::ASTNodeMultiVariableDecl *>(node); multiVariableDecl != nullptr) {
            if (!this->validateNodes(multiVariableDecl->getVariables()))
                return false;
        } else if (const auto typeDeclNode = dynamic_cast<ast::ASTNodeTypeDecl *>(node); typeDeclNode != nullptr) {
            if (!typeDeclNode->isForwardDeclared())
                if (!this->validateNode(typeDeclNode->getType()))
                    return false;
        } else if (const auto structNode = dynamic_cast<ast::ASTNodeStruct *>(node); structNode != nullptr) {
            if (!this->validateNodes(structNode->getMembers()))
                return false;
        } else if (const auto unionNode = dynamic_cast<ast::ASTNodeUnion *>(node); unionNode != nullptr) {
            if (!this->validateNodes(unionNode->getMembers()))
                return false;
        } else if (const auto bitfieldNode = dynamic_cast<ast::ASTNodeBitfield *>(node); bitfieldNode != nullptr) {
            if (!this->validateNodes(bitfieldNode->getEntries()))
                return false;
        } else if (const auto enumNode = dynamic_cast<ast::ASTNodeEnum *>(node); enumNode != nullptr) {
            std::unordered_set<std::string> enumIdentifiers;
            for (const auto &[name, value] : enumNode->getEntries()) {
                if (!enumIdentifiers.insert(name).second) {
                    error("Redeclaration of enum entry '{}'", name);
                    return false;
                }
            }
        } else if (const auto conditionalNode = dynamic_cast<ast::ASTNodeConditionalStatement *>(node); conditionalNode != nullptr) {
            const auto prevIdentifiers = identifiers;
            if (!this->validateNodes(conditionalNode->getTrueBody(), false))
                return false;
            identifiers = prevIdentifiers;
            if (!this->validateNodes(conditionalNode->getFalseBody(), false))
                return false;
            identifiers = prevIdentifiers;
        } else if (const auto functionNode = dynamic_cast<ast::ASTNodeFunctionDefinition *>(node); functionNode != nullptr) {
            if (!identifiers.insert(functionNode->getName()).second) {
                error("Redefinition of identifier '{}'", functionNode->getName());
                return false;
            }

            std::unordered_set<std::string> parameterIdentifiers;
            for (const auto &[name, type] : functionNode->getParams()) {
                if (!parameterIdentifiers.insert(name).second) {
                    error("Redefinition of function parameter '{}'", name);
                    return false;
                }
            }
        }

        this->m_validatedNodes.insert(node);
        return true;
    }

    Location Validator::location() {
        return m_lastNode->getLocation();
    }


}