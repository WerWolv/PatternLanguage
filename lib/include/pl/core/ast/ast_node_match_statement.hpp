#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    struct MatchCase {
        std::unique_ptr<ASTNode> condition;
        std::vector<std::unique_ptr<ASTNode>> body;

        MatchCase() = default;

        MatchCase(std::unique_ptr<ASTNode> condition, std::vector<std::unique_ptr<ASTNode>> body)
            : condition(std::move(condition)), body(std::move(body)) { }

        MatchCase(const MatchCase &other) {
            this->condition = other.condition->clone();
            for (auto &statement : other.body)
                this->body.push_back(statement->clone());
        }

        MatchCase(MatchCase &&other) noexcept {
            this->condition = std::move(other.condition);
            this->body = std::move(other.body);
        }

        MatchCase &operator=(const MatchCase &other) {
            this->condition = other.condition->clone();
            for (auto &statement : other.body)
                this->body.push_back(statement->clone());
            return *this;
        }
    };

    class ASTNodeMatchStatement : public ASTNode {

    public:
        explicit ASTNodeMatchStatement(std::vector<MatchCase> cases, std::optional<MatchCase> defaultCase);
        ASTNodeMatchStatement(const ASTNodeMatchStatement &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMatchStatement(*this));
        }

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        FunctionResult execute(Evaluator *evaluator) const override;

    private:
        [[nodiscard]] bool evaluateCondition(const std::unique_ptr<ASTNode> &condition, Evaluator *evaluator) const;
        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>* getCaseBody(Evaluator *evaluator) const;

        std::vector<MatchCase> m_cases;
        std::optional<MatchCase> m_defaultCase;
    };
}