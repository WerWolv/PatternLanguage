#pragma once

#include <pl/core/ast/visitors/ast_validator.hpp>

namespace pl::core {
    class ControlFlowValidator final : public ASTValidator {
        uint32_t m_loopsCount = 0;

        void reset() override;

        void visit(ast::ASTNodeFunctionDefinition& node) override;
        void visit(ast::ASTNodeWhileStatement& node) override;
        void visit(ast::ASTNodeMatchStatement& node) override;
        void visit(ast::ASTNodeTryCatchStatement& node) override;
        void visit(ast::ASTNodeConditionalStatement& node) override;
        void visit(ast::ASTNodeControlFlowStatement& node) override;
    };
}