#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_appilication.hpp>

namespace pl::core::ast {

    class ASTNodeCast : public ASTNode {
    public:
        ASTNodeCast(std::unique_ptr<ASTNode> &&value, std::unique_ptr<ASTNodeTypeApplication> &&type, bool reinterpret);
        ASTNodeCast(const ASTNodeCast &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeCast(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

        [[nodiscard]] const std::unique_ptr<ASTNode> &getValue() const {
            return m_value;
        }
        [[nodiscard]] const std::unique_ptr<ASTNodeTypeApplication> &getType() const {
            return m_type;
        }

        void accept(vis::ASTVisitor &v) override {
            v.visit(*this);
        }
    private:
        std::unique_ptr<ASTNode> castValue(const Token::Literal &literal, Token::ValueType type, const std::shared_ptr<ptrn::Pattern> &typePattern, Evaluator *evaluator) const;

    private:
        std::unique_ptr<ASTNode> m_value;
        std::unique_ptr<ASTNodeTypeApplication> m_type;
        bool m_reinterpret;
    };

}