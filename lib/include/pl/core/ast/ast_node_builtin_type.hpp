#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeBuiltinType : public ASTNode {
    public:
        explicit ASTNodeBuiltinType(Token::ValueType type);

        [[nodiscard]] constexpr const Token::ValueType& getType() const {
            return this->m_type;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBuiltinType(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;

    private:
        Token::ValueType m_type;
    };

}