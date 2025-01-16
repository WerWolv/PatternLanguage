#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeTryCatchStatement : public ASTNode {
    public:
        explicit ASTNodeTryCatchStatement(std::vector<std::unique_ptr<ASTNode>> &&tryBody, std::vector<std::unique_ptr<ASTNode>> &&catchBody);
        ASTNodeTryCatchStatement(const ASTNodeTryCatchStatement &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTryCatchStatement(*this));
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getTryBody() const {
            return this->m_tryBody;
        }
        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getCatchBody() const {
            return this->m_catchBody;
        }

    private:

        std::vector<std::unique_ptr<ASTNode>> m_tryBody, m_catchBody;
    };

}