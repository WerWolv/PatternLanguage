#pragma once

#include <pl/core/ast/visitors/ast_visitor.hpp>
#include <pl/core/errors/error.hpp>
#include <pl/core/errors/result.hpp>

#include <wolv/utils/core.hpp>

#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>

namespace pl::core {

    namespace ast { class ASTNode; }

    class ASTValidator : public err::ErrorCollector, ast::vis::ASTVisitor {
        using Result = hlp::CompileResult<bool>;
    public:
        ASTValidator() = default;

        Result validate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast);

    protected:
        virtual void reset() = 0;

        // Helpers
        template <ast::vis::Acceptable T>
        void visitTypeErased(const T& node) {
            auto node_ptr = node.get();
            if (m_validatedNodes.contains(node_ptr)) {
                // Seen this one, let's not repeat ourselves...
                return;
            }
            if (node_ptr == nullptr) {
                errorDesc("Null-Pointer found in AST.", "This is a parser bug. Please report it on GitHub.");
                return;
            }
            m_lastNode = node_ptr;
            node_ptr->accept(*this);
        }
        template <ast::vis::Acceptable TypedNode>
        void visitTyped(const TypedNode& node) {
            auto node_ptr = node.get();
            if (m_validatedNodes.contains(node_ptr)) {
                // Seen this one, let's not repeat ourselves...
                return;
            }
            if (node_ptr == nullptr) {
                errorDesc("Null-Pointer found in AST.", "This is a parser bug. Please report it on GitHub.");
                return;
            }
            m_lastNode = node_ptr;
            visit(*node_ptr);
        }

        template<ast::vis::Acceptable T>
        void visitTypeErasedNodes(const std::vector<T>& nodes) {
            for (const auto& node_like: nodes) {
                visitTypeErased(node_like);
            }
        }
        template<ast::vis::Acceptable T>
        void visitTypedNodes(const std::vector<T>& nodes) {
            for (const auto& node_like: nodes) {
                visitTyped(node_like);
            }
        }


        // No default action
        void visit(ast::ASTNode& node) override { wolv::util::unused(node); };
        void visit(ast::ASTNodeArrayVariableDecl& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeAttribute& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeBitfield& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeBitfieldArrayVariableDecl& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeBitfieldField& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeBitfieldFieldSigned& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeBitfieldFieldSizedType& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeBuiltinType& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeCast& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeCompoundStatement& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeConditionalStatement& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeControlFlowStatement& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeEnum& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeFunctionCall& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeFunctionDefinition& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeImportedType& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeLiteral& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeLValueAssignment& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeMatchStatement& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeMathematicalExpression& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeMultiVariableDecl& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeParameterPack& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodePointerVariableDecl& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeRValue& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeRValueAssignment& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeScopeResolution& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeStruct& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeTemplateParameter& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeTernaryExpression& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeTryCatchStatement& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeTypeApplication& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeTypeDecl& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeTypeOperator& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeUnion& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeVariableDecl& node) override { wolv::util::unused(node); }
        void visit(ast::ASTNodeWhileStatement& node) override { wolv::util::unused(node); }

        Location location() override;

        ast::ASTNode *m_lastNode = nullptr;
        std::set<ast::ASTNode*> m_validatedNodes;
    };

}
