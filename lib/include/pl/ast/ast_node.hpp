#pragma once

#include <pl/token.hpp>
#include <pl/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/errors/evaluator_errors.hpp>

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

        [[nodiscard]] constexpr u32 getLine() const { return this->m_line; }
        [[nodiscard]] constexpr u32 getColumn() const { return this->m_column; }

        [[maybe_unused]] constexpr void setSourceLocation(u32 line, u32 column) {
            this->m_line = line;
            this->m_column = column;
        }

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

            err::E0001.throwError("Cannot execute non-functional statement.", "This is a evaluator bug!", this);
        }

    private:
        u32 m_line = 1;
        u32 m_column = 1;
    };

}