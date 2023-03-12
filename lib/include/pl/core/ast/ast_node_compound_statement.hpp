#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeCompoundStatement : public ASTNode {
    public:
        explicit ASTNodeCompoundStatement(std::vector<std::unique_ptr<ASTNode>> &&statements, bool newScope = false) : m_statements(std::move(statements)), m_newScope(newScope) {
        }

        ASTNodeCompoundStatement(const ASTNodeCompoundStatement &other) : ASTNode(other) {
            for (const auto &statement : other.m_statements) {
                this->m_statements.push_back(statement->clone());
            }

            this->m_newScope = other.m_newScope;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeCompoundStatement(*this));
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& getStatements() const { return this->m_statements; }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            std::unique_ptr<ASTNode> result = nullptr;

            for (const auto &statement : this->m_statements) {
                result = statement->evaluate(evaluator);
            }

            return result;
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            std::vector<std::shared_ptr<ptrn::Pattern>> result;

            for (const auto &statement : this->m_statements) {
                auto patterns = statement->createPatterns(evaluator);
                std::move(patterns.begin(), patterns.end(), std::back_inserter(result));
            }

            return result;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            FunctionResult result;
            auto variables         = *evaluator->getScope(0).scope;

            auto scopeGuard = SCOPE_GUARD {
                evaluator->popScope();
            };

            if (this->m_newScope) {
                evaluator->pushScope(nullptr, variables);
            } else {
                scopeGuard.release();
            }

            for (const auto &statement : this->m_statements) {
                result = statement->execute(evaluator);
                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;
            }

            return result;
        }

    public:
        std::vector<std::unique_ptr<ASTNode>> m_statements;
        bool m_newScope = false;
    };

}