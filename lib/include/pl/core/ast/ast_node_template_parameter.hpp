#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeTemplateParameter : public ASTNode {
    public:
        explicit ASTNodeTemplateParameter(Token::Identifier name, bool isType)
            : m_name(std::move(name)), m_isType(isType) {}

        ASTNodeTemplateParameter(const ASTNodeTemplateParameter &) = default;

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTemplateParameter(*this));
        }

        [[nodiscard]] const auto &getName() const {
            return this->m_name;
        }

        [[nodiscard]] bool isType() const {
            return this->m_isType;
        }

    private:
        Token::Identifier m_name;
        bool m_isType;
    };

}