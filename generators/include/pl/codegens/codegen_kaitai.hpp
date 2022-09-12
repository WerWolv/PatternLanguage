#pragma once

#include <pl/pattern_language.hpp>

#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>

#include <string>
#include <utility>

namespace pl::gen::code {

    class CodegenKaitai : public Codegen {
    public:
        CodegenKaitai() : Codegen("kaitai") { }
        ~CodegenKaitai() override = default;

        [[nodiscard]] std::string getFileExtension() const override { return ".kys"; }
        [[nodiscard]] std::string generate(const PatternLanguage &runtime, const std::vector<std::shared_ptr<pl::core::ast::ASTNode>> &ast) override {
            std::string result;

            auto evaluator = runtime.getInternals().evaluator;

            std::vector<std::pair<std::unique_ptr<core::ast::ASTNode>, std::unique_ptr<core::ast::ASTNode>>>
                    enums, structs;

            for (const auto &node : ast) {
                if (auto typeNode = dynamic_cast<core::ast::ASTNodeTypeDecl*>(node.get()); typeNode != nullptr) {
                    auto resolvedTypeNode = resolveTypeNode(evaluator, node->clone());

                    if (dynamic_cast<core::ast::ASTNodeEnum*>(resolvedTypeNode.get())) {
                        enums.emplace_back(typeNode->clone(), std::move(resolvedTypeNode));
                    } else if (dynamic_cast<core::ast::ASTNodeStruct*>(resolvedTypeNode.get())) {
                        structs.emplace_back(typeNode->clone(), std::move(resolvedTypeNode));
                    }
                }
            }

            result += "enums:\n";
            for (const auto &[typeNode, enumNode] : enums) {
                result += fmt::format("  {}:\n", static_cast<core::ast::ASTNodeTypeDecl*>(typeNode.get())->getName());
                for (const auto &[name, values] : static_cast<core::ast::ASTNodeEnum*>(enumNode.get())->getEntries()) {
                    result += fmt::format("    {}: {}\n", core::Token::literalToUnsigned(*resolveLiteral(evaluator, values.first)), name);
                }
            }

            result += "\n";

            result += "types:\n";
            for (const auto &[typeNode, structNode] : structs) {
                result += fmt::format("  {}:\n", static_cast<core::ast::ASTNodeTypeDecl*>(typeNode.get())->getName());
                result += "    seq:\n";
                for (const auto &member : static_cast<core::ast::ASTNodeStruct*>(structNode.get())->getMembers()) {
                    if (auto variableNode = dynamic_cast<core::ast::ASTNodeVariableDecl*>(member.get())) {
                        result += fmt::format("      - id: {}\n", variableNode->getName());
                        result += fmt::format("        type: {}\n", variableNode->getType()->getName());
                    }
                }
            }

            return result;
        }

    private:
        std::unique_ptr<core::ast::ASTNode> resolveTypeNode(core::Evaluator *evaluator, const std::unique_ptr<core::ast::ASTNode> &node) {
            if (dynamic_cast<core::ast::ASTNodeTypeDecl*>(node.get()) != nullptr)
                return resolveTypeNode(evaluator, node->evaluate(evaluator));
            else
                return node->clone();
        }

        std::optional<core::Token::Literal> resolveLiteral(core::Evaluator *evaluator, const std::unique_ptr<core::ast::ASTNode> &node) {
            if (dynamic_cast<core::ast::ASTNodeMathematicalExpression*>(node.get()) != nullptr)
                return resolveLiteral(evaluator, node->evaluate(evaluator));
            else if (auto literalNode = dynamic_cast<core::ast::ASTNodeLiteral*>(node.get()); literalNode != nullptr)
                return literalNode->getValue();
            else
                return std::nullopt;
        }
    };

}