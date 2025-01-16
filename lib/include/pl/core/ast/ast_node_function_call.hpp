#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeFunctionCall : public ASTNode {
    public:
        explicit ASTNodeFunctionCall(std::string functionName, std::vector<std::unique_ptr<ASTNode>> &&params);

        ASTNodeFunctionCall(const ASTNodeFunctionCall &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeFunctionCall(*this));
        }

        [[nodiscard]] const std::string &getFunctionName() const {
            return this->m_functionName;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getParams() const {
            return this->m_params;
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

    private:
        std::string m_functionName;
        std::vector<std::unique_ptr<ASTNode>> m_params;
    };

}