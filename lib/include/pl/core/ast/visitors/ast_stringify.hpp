#pragma once

#include <pl/core/ast/visitors/ast_visitor.hpp>

#include <memory>
#include <string>
#include <vector>

namespace pl::core::ast {

    class ASTNode;
    class ASTNodeTemplateParameter;

    // Renders expressions and type names back into Pattern Language source.
    // All node dispatching goes through the vis::ASTVisitor framework so only the
    // node types that can appear inside an expression or type name need to know
    // how to print themselves, everything else falls back to visit(ASTNode&).
    class ASTStringifier : public vis::ASTVisitor {
    public:
        using TemplateParameters = std::vector<std::shared_ptr<ASTNodeTemplateParameter>>;

        // Prints an expression, wrapping it in parentheses when its precedence demands it.
        // A parenDepth of 0 means template argument context where '<' and '>' need to be
        // disambiguated from the template argument list itself.
        //
        // Unsigned literals keep their 'u' suffix so the printed source re-parses to the same
        // literal. Callers that build display strings (pattern type names) instead of source
        // can turn this off to keep their output unchanged from previous releases.
        explicit ASTStringifier(bool keepUnsignedSuffix = true) : m_keepUnsignedSuffix(keepUnsignedSuffix) {}

        std::string stringifyExpression(const ASTNode *node, int parenDepth = 0, const TemplateParameters *parentParams = nullptr);

        // Prints a type name, either a builtin type, a type declaration or a type
        // application with its template arguments.
        std::string stringifyTypeName(const ASTNode *type, const TemplateParameters *parentParams = nullptr);

        // vis::ASTVisitor
        void visit(ASTNode &node) override;
        void visit(ASTNodeArrayVariableDecl &node) override;
        void visit(ASTNodeAttribute &node) override;
        void visit(ASTNodeBitfield &node) override;
        void visit(ASTNodeBitfieldArrayVariableDecl &node) override;
        void visit(ASTNodeBitfieldField &node) override;
        void visit(ASTNodeBitfieldFieldSigned &node) override;
        void visit(ASTNodeBitfieldFieldSizedType &node) override;
        void visit(ASTNodeBuiltinType &node) override;
        void visit(ASTNodeCast &node) override;
        void visit(ASTNodeCompoundStatement &node) override;
        void visit(ASTNodeConditionalStatement &node) override;
        void visit(ASTNodeControlFlowStatement &node) override;
        void visit(ASTNodeEnum &node) override;
        void visit(ASTNodeFunctionCall &node) override;
        void visit(ASTNodeFunctionDefinition &node) override;
        void visit(ASTNodeImportedType &node) override;
        void visit(ASTNodeLiteral &node) override;
        void visit(ASTNodeLValueAssignment &node) override;
        void visit(ASTNodeMatchStatement &node) override;
        void visit(ASTNodeMathematicalExpression &node) override;
        void visit(ASTNodeMultiVariableDecl &node) override;
        void visit(ASTNodeParameterPack &node) override;
        void visit(ASTNodePointerVariableDecl &node) override;
        void visit(ASTNodeRValue &node) override;
        void visit(ASTNodeRValueAssignment &node) override;
        void visit(ASTNodeScopeResolution &node) override;
        void visit(ASTNodeStruct &node) override;
        void visit(ASTNodeTemplateParameter &node) override;
        void visit(ASTNodeTernaryExpression &node) override;
        void visit(ASTNodeTryCatchStatement &node) override;
        void visit(ASTNodeTypeApplication &node) override;
        void visit(ASTNodeTypeDecl &node) override;
        void visit(ASTNodeTypeOperator &node) override;
        void visit(ASTNodeUnion &node) override;
        void visit(ASTNodeVariableDecl &node) override;
        void visit(ASTNodeWhileStatement &node) override;

    private:
        // Describes the position a node is printed in
        struct Context {
            // How deeply the node is nested inside parentheses and brackets. A depth of 0
            // means template argument context where comparison operators are ambiguous.
            int parenDepth = 0;
            // The minimum precedence a node needs to stay unparenthesized
            int minPrecedence = 0;
            // Whether a type name is being printed instead of an expression
            bool typeName = false;
            // Template parameters of the type the printed node belongs to
            const TemplateParameters *parentParams = nullptr;
        };

        std::string stringify(const ASTNode *node, const Context &context);

        // Records the precedence of the node that is being visited and whether it needs
        // to parenthesize itself
        void beginNode(int precedence, bool angleAmbiguous = false);

        // The context to print a child node in. Children sit one level deeper whenever
        // the node currently being visited parenthesizes itself.
        [[nodiscard]] Context childContext(int minPrecedence, int depthOffset = 0) const;

        Context m_context;
        std::string m_result;
        int m_precedence = 0;
        bool m_parenthesized = false;
        bool m_keepUnsignedSuffix = true;
    };

}
