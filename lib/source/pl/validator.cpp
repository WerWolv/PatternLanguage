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

    bool Validator::validate(const std::vector<std::shared_ptr<ASTNode>> &ast) {
        std::unordered_set<std::string> identifiers;
        std::unordered_set<std::string> types;

        try {

            for (const auto &node : ast) {
                if (node == nullptr)
                    throwValidatorError("nullptr in AST. This is a bug!", 1);

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl *>(node.get()); variableDeclNode != nullptr) {
                    if (!identifiers.insert(variableDeclNode->getName().data()).second)
                        throwValidatorError(fmt::format("redefinition of identifier '{0}'", variableDeclNode->getName().data()), variableDeclNode->getLineNumber());

                    this->validate(pl::moveToVector<std::shared_ptr<ASTNode>>(variableDeclNode->getType()->clone()));
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl *>(node.get()); typeDeclNode != nullptr) {
                    if (!types.insert(typeDeclNode->getName().data()).second)
                        throwValidatorError(fmt::format("redefinition of type '{0}'", typeDeclNode->getName().data()), typeDeclNode->getLineNumber());

                    if (!typeDeclNode->isForwardDeclared())
                        this->validate(pl::moveToVector<std::shared_ptr<ASTNode>>(typeDeclNode->getType()->clone()));
                } else if (auto structNode = dynamic_cast<ASTNodeStruct *>(node.get()); structNode != nullptr) {
                    this->validate(structNode->getMembers());
                } else if (auto unionNode = dynamic_cast<ASTNodeUnion *>(node.get()); unionNode != nullptr) {
                    this->validate(unionNode->getMembers());
                } else if (auto enumNode = dynamic_cast<ASTNodeEnum *>(node.get()); enumNode != nullptr) {
                    std::unordered_set<std::string> enumIdentifiers;
                    for (auto &[name, value] : enumNode->getEntries()) {
                        if (!enumIdentifiers.insert(name).second)
                            throwValidatorError(fmt::format("redefinition of enum constant '{0}'", name.c_str()), value->getLineNumber());
                    }
                }
            }

        } catch (PatternLanguageError &e) {
            this->m_error = e;
            return false;
        }

        return true;
    }
}