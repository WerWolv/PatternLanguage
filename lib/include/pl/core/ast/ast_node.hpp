#pragma once

#include <pl/core/token.hpp>
#include <pl/core/location.hpp>

#include <pl/core/errors/runtime_errors.hpp>
#include <pl/helpers/concepts.hpp>

#include <optional>
#include <string>

namespace pl::ptrn { class Pattern; }
namespace pl::core { class Evaluator; }

namespace pl::core::ast {

    class ASTNode : public Cloneable<ASTNode> {
    public:
        using FunctionResult = std::optional<Token::Literal>;

        constexpr ASTNode() = default;
        constexpr virtual ~ASTNode() = default;
        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] const Location& getLocation() const;
        void setLocation(const Location &location);

        [[nodiscard]] virtual std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const;
        [[nodiscard]] virtual std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const;
        virtual FunctionResult execute(Evaluator *evaluator) const;

        void setDocComment(const std::string &comment);
        [[nodiscard]] const std::string &getDocComment() const;
        void setShouldDocument(bool shouldDocument);
        [[nodiscard]] bool shouldDocument() const;

    private:
        Location m_location = Location::Empty();

        std::string m_docComment;
        bool m_document = false;
    };

}