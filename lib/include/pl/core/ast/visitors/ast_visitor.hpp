#pragma once

#include <utility>
#include <vector>

namespace pl::core::ast {
    // Forward declarations
    class ASTNode;
    class ASTNodeArrayVariableDecl;
    class ASTNodeAttribute;
    class ASTNodeBitfield;
    class ASTNodeBitfieldArrayVariableDecl;
    class ASTNodeBitfieldField;
    class ASTNodeBitfieldFieldSigned;
    class ASTNodeBitfieldFieldSizedType;
    class ASTNodeBuiltinType;
    class ASTNodeCast;
    class ASTNodeCompoundStatement;
    class ASTNodeConditionalStatement;
    class ASTNodeControlFlowStatement;
    class ASTNodeEnum;
    class ASTNodeFunctionCall;
    class ASTNodeFunctionDefinition;
    class ASTNodeImportedType;
    class ASTNodeLiteral;
    class ASTNodeLValueAssignment;
    class ASTNodeMatchStatement;
    class ASTNodeMathematicalExpression;
    class ASTNodeMultiVariableDecl;
    class ASTNodeParameterPack;
    class ASTNodePointerVariableDecl;
    class ASTNodeRValue;
    class ASTNodeRValueAssignment;
    class ASTNodeScopeResolution;
    class ASTNodeStruct;
    class ASTNodeTemplateParameter;
    class ASTNodeTernaryExpression;
    class ASTNodeTryCatchStatement;
    class ASTNodeTypeApplication;
    class ASTNodeTypeDecl;
    class ASTNodeTypeOperator;
    class ASTNodeUnion;
    class ASTNodeVariableDecl;
    class ASTNodeWhileStatement;


    namespace vis {
        class ASTVisitor;

        template <typename T, typename Base>
        concept PointerToDerivedFrom = std::is_pointer_v<T> &&
        std::derived_from<std::remove_pointer_t<T>, Base>;

        template <typename T>
        concept Acceptable = requires(T obj) {
            static_cast<bool>(obj);
            { obj.get() } -> PointerToDerivedFrom<ASTNode>;
            (*obj).accept(std::declval<ASTVisitor&>());
        };

        class ASTVisitor {
        public:
            virtual ~ASTVisitor() = default;

            // Overrideable
            virtual void visit(ASTNode& node) = 0;
            virtual void visit(ASTNodeArrayVariableDecl& node) = 0;
            virtual void visit(ASTNodeAttribute& node) = 0;
            virtual void visit(ASTNodeBitfield& node) = 0;
            virtual void visit(ASTNodeBitfieldArrayVariableDecl& node) = 0;
            virtual void visit(ASTNodeBitfieldField& node) = 0;
            virtual void visit(ASTNodeBitfieldFieldSigned& node) = 0;
            virtual void visit(ASTNodeBitfieldFieldSizedType& node) = 0;
            virtual void visit(ASTNodeBuiltinType& node) = 0;
            virtual void visit(ASTNodeCast& node) = 0;
            virtual void visit(ASTNodeCompoundStatement& node) = 0;
            virtual void visit(ASTNodeConditionalStatement& node) = 0;
            virtual void visit(ASTNodeControlFlowStatement& node) = 0;
            virtual void visit(ASTNodeEnum& node) = 0;
            virtual void visit(ASTNodeFunctionCall& node) = 0;
            virtual void visit(ASTNodeFunctionDefinition& node) = 0;
            virtual void visit(ASTNodeImportedType& node) = 0;
            virtual void visit(ASTNodeLiteral& node) = 0;
            virtual void visit(ASTNodeLValueAssignment& node) = 0;
            virtual void visit(ASTNodeMatchStatement& node) = 0;
            virtual void visit(ASTNodeMathematicalExpression& node) = 0;
            virtual void visit(ASTNodeMultiVariableDecl& node) = 0;
            virtual void visit(ASTNodeParameterPack& node) = 0;
            virtual void visit(ASTNodePointerVariableDecl& node) = 0;
            virtual void visit(ASTNodeRValue& node) = 0;
            virtual void visit(ASTNodeRValueAssignment& node) = 0;
            virtual void visit(ASTNodeScopeResolution& node) = 0;
            virtual void visit(ASTNodeStruct& node) = 0;
            virtual void visit(ASTNodeTemplateParameter& node) = 0;
            virtual void visit(ASTNodeTernaryExpression& node) = 0;
            virtual void visit(ASTNodeTryCatchStatement& node) = 0;
            virtual void visit(ASTNodeTypeApplication& node) = 0;
            virtual void visit(ASTNodeTypeDecl& node) = 0;
            virtual void visit(ASTNodeTypeOperator& node) = 0;
            virtual void visit(ASTNodeUnion& node) = 0;
            virtual void visit(ASTNodeVariableDecl& node) = 0;
            virtual void visit(ASTNodeWhileStatement& node) = 0;
        };
    }
}
