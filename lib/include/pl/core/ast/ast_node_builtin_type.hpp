#pragma once

#include <pl/api.hpp>
#include <pl/core/ast/ast_node.hpp>
#include <functional>

namespace pl::core::ast {

    class ASTNodeBuiltinType : public ASTNode {
    public:
        explicit ASTNodeBuiltinType(Token::ValueType type);
        explicit ASTNodeBuiltinType(api::TypeCallback callback)
        : m_type(Token::ValueType::CustomType), m_customTypeCallback(std::move(callback)) {}

        [[nodiscard]] constexpr const Token::ValueType& getType() const {
            return this->m_type;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBuiltinType(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;

    private:
        Token::ValueType m_type;
        api::TypeCallback m_customTypeCallback;
    };

}
