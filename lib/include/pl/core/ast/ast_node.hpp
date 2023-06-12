#pragma once

#include <pl/core/token.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/core/errors/evaluator_errors.hpp>

#include <pl/helpers/utils.hpp>
#include <pl/helpers/concepts.hpp>

#include <wolv/utils/guards.hpp>

namespace pl::ptrn { class Pattern; }

namespace pl::core::ast {

    class ASTNode : public Cloneable<ASTNode> {
    public:
        using FunctionResult = std::optional<Token::Literal>;

        constexpr ASTNode() = default;
        constexpr virtual ~ASTNode() = default;
        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] u32 getLine() const;
        [[nodiscard]] u32 getColumn() const;
        [[maybe_unused]] void setSourceLocation(u32 line, u32 column);

        [[nodiscard]] virtual std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const;
        [[nodiscard]] virtual std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const;
        virtual FunctionResult execute(Evaluator *evaluator) const;

        void setDocComment(const std::string &comment);
        [[nodiscard]] const std::string &getDocComment() const;
        void setShouldDocument(bool shouldDocument);
        [[nodiscard]] bool shouldDocument() const;

    private:
        u32 m_line = 1;
        u32 m_column = 1;

        std::string m_docComment;
        bool m_document = false;
    };

}