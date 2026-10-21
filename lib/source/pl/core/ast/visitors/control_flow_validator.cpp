
#include <pl/core/ast/visitors/control_flow_validator.hpp>

#include <pl/core/ast/ast_node_function_definition.hpp>
#include <pl/core/ast/ast_node_control_flow_statement.hpp>
#include <pl/core/ast/ast_node_try_catch_statement.hpp>
#include <pl/core/ast/ast_node_while_statement.hpp>
#include <pl/core/ast/ast_node_match_statement.hpp>
#include <pl/core/ast/ast_node_conditional_statement.hpp>

namespace pl::core {
    void ControlFlowValidator::visit(ast::ASTNodeFunctionDefinition &node) {
        visitTypeErasedNodes(node.getBody());
    }

    void ControlFlowValidator::visit(ast::ASTNodeWhileStatement &node) {
        ++m_loopsCount;
        visitTypeErasedNodes(node.getBody());
        --m_loopsCount;
    }

    void ControlFlowValidator::visit(ast::ASTNodeMatchStatement &node) {
        if (const auto& default_case = node.getDefaultCase(); default_case) {
            const auto& body = (*default_case).body;
            visitTypeErasedNodes(body);
        }
        for (const auto& match_case: node.getNonDefaultCases()) {
            visitTypeErasedNodes(match_case.body);
        }
    }

    void ControlFlowValidator::visit(ast::ASTNodeTryCatchStatement &node) {
        visitTypeErasedNodes(node.getTryBody());
        visitTypeErasedNodes(node.getCatchBody());
    }

    void ControlFlowValidator::visit(ast::ASTNodeConditionalStatement &node) {
        visitTypeErasedNodes(node.getTrueBody());
        visitTypeErasedNodes(node.getFalseBody());
    }

    void ControlFlowValidator::visit(ast::ASTNodeControlFlowStatement &node) {
        if (m_loopsCount > 0) {
            // Nothing bad.
            return;
        }
        // Could be bad, let's check

        auto type = node.getType();
        switch (type) {
            case ControlFlowStatement::Break: {
                error("Break statements can only be used within a loop.");
                return;
            }
            case ControlFlowStatement::Continue: {
                error("Continue statements can only be used within a loop.");
                return;
            }
            default: {
                return;
            }
        }
    }

    void ControlFlowValidator::reset() {
        // This can't become anything else, but why not make it explicit...
        m_loopsCount = 0;
    }
}