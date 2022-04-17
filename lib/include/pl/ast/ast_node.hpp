#pragma once

#include <pl/token.hpp>
#include <pl/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include "helpers/utils.hpp"
#include "helpers/guards.hpp"
#include "helpers/concepts.hpp"

namespace pl {

    class Pattern;
    class Evaluator;

    class ASTNode : public Cloneable<ASTNode> {
    public:
        constexpr ASTNode() = default;

        constexpr virtual ~ASTNode() = default;

        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }

        [[maybe_unused]] constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        [[nodiscard]] virtual std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const {
            pl::unused(evaluator);

            return this->clone();
        }

        [[nodiscard]] virtual std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const {
            pl::unused(evaluator);

            return {};
        }

        using FunctionResult = std::optional<Token::Literal>;
        virtual FunctionResult execute(Evaluator *evaluator) const {
            pl::unused(evaluator);

            LogConsole::abortEvaluation("cannot execute non-function statement", this);
        }

    private:
        u32 m_lineNumber = 1;
    };

}