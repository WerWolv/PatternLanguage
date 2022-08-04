#include "pl/validator.hpp"

#include "pl/ast/ast_node.hpp"
#include "pl/ast/ast_node_variable_decl.hpp"
#include "pl/ast/ast_node_type_decl.hpp"
#include "pl/ast/ast_node_struct.hpp"
#include "pl/ast/ast_node_union.hpp"
#include "pl/ast/ast_node_enum.hpp"

#include <unordered_set>
#include <string>

namespace pl {

    bool Validator::validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ASTNode>> &ast) {
        std::unordered_set<std::string> identifiers;
        std::unordered_set<std::string> types;

        ASTNode *lastNode = nullptr;
        try {

            for (const auto &node : ast) {
                if (node == nullptr)
                    err::V0001.throwError("Null-Pointer found in AST.", "This is a parser bug. Please report it on GitHub.");

                lastNode = node.get();

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl *>(node.get()); variableDeclNode != nullptr) {
                    if (!identifiers.insert(variableDeclNode->getName().data()).second)
                        err::V0002.throwError(fmt::format("Redefinition of variable '{0}", variableDeclNode->getName()));

                    this->validate(sourceCode, pl::moveToVector<std::shared_ptr<ASTNode>>(variableDeclNode->getType()->clone()));
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl *>(node.get()); typeDeclNode != nullptr) {
                    if (!types.insert(typeDeclNode->getName().c_str()).second)
                        err::V0002.throwError(fmt::format("Redefinition of type '{0}", typeDeclNode->getName()));

                    if (!typeDeclNode->isForwardDeclared())
                        this->validate(sourceCode, pl::moveToVector<std::shared_ptr<ASTNode>>(typeDeclNode->getType()->clone()));
                } else if (auto structNode = dynamic_cast<ASTNodeStruct *>(node.get()); structNode != nullptr) {
                    this->validate(sourceCode, structNode->getMembers());
                } else if (auto unionNode = dynamic_cast<ASTNodeUnion *>(node.get()); unionNode != nullptr) {
                    this->validate(sourceCode, unionNode->getMembers());
                } else if (auto enumNode = dynamic_cast<ASTNodeEnum *>(node.get()); enumNode != nullptr) {
                    std::unordered_set<std::string> enumIdentifiers;
                    for (auto &[name, value] : enumNode->getEntries()) {
                        if (!enumIdentifiers.insert(name).second)
                            err::V0002.throwError(fmt::format("Redefinition of enum entry '{0}", name));
                    }
                }
            }

        } catch (err::Error &e) {
            if (lastNode != nullptr)
                this->m_error = err::Error::Exception(e.format(sourceCode, lastNode->getLine(), lastNode->getColumn()), lastNode->getLine(), lastNode->getColumn());
            else
                this->m_error = err::Error::Exception(e.format(sourceCode, 1, 1), 1, 1);

            return false;
        }

        return true;
    }
}