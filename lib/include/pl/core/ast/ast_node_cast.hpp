#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

namespace pl::core::ast {

    class ASTNodeCast : public ASTNode {
    public:
        ASTNodeCast(std::unique_ptr<ASTNode> &&value, std::unique_ptr<ASTNode> &&type);
        ASTNodeCast(const ASTNodeCast &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeCast(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

    private:
        std::unique_ptr<ASTNode> castValue(const Token::Literal &literal, Token::ValueType type, const std::shared_ptr<ptrn::Pattern> &typePattern, Evaluator *evaluator) const;

    private:
        std::unique_ptr<ASTNode> m_value;
        std::unique_ptr<ASTNode> m_type;
    };

}