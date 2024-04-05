#include <pl/core/ast/ast_node_enum.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_enum.hpp>

namespace pl::core::ast {

    ASTNodeEnum::ASTNodeEnum(std::unique_ptr<ASTNode> &&underlyingType) : ASTNode(), m_underlyingType(std::move(underlyingType)) { }

    ASTNodeEnum::ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other), Attributable(other) {
        for (const auto &[name, expr] : other.getEntries()) {
            this->m_entries[name] = { expr.first->clone(), expr.second->clone() };
        }
        this->m_underlyingType = other.m_underlyingType->clone();
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeEnum::createPatterns(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        evaluator->alignToByte();

        const auto nodes = this->m_underlyingType->createPatterns(evaluator);
        if (nodes.empty())
            err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());
        auto &underlying = nodes.front();

        auto pattern = std::make_shared<ptrn::PatternEnum>(evaluator, underlying->getOffset(), 0, getLocation().line);

        pattern->setSection(evaluator->getSectionId());

        std::vector<ptrn::PatternEnum::EnumValue> enumEntries;
        for (const auto &[name, expr] : this->m_entries) {
            auto &[min, max] = expr;

            const auto minNode = min->evaluate(evaluator);
            const auto maxNode = max->evaluate(evaluator);

            const auto minLiteral = dynamic_cast<ASTNodeLiteral *>(minNode.get());
            const auto maxLiteral = dynamic_cast<ASTNodeLiteral *>(maxNode.get());

            if (minLiteral == nullptr || maxLiteral == nullptr)
                err::E0010.throwError("Cannot use void expression as enum value.", {}, this->getLocation());

            // Check that the enum values can be converted to integers
            (void)minLiteral->getValue().toUnsigned();
            (void)maxLiteral->getValue().toUnsigned();

            enumEntries.push_back(ptrn::PatternEnum::EnumValue{
                    minLiteral->getValue(),
                    maxLiteral->getValue(),
                    name
            });
        }

        pattern->setEnumValues(enumEntries);

        pattern->setSize(underlying->getSize());
        pattern->setEndian(underlying->getEndian());

        applyTypeAttributes(evaluator, this, pattern);

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

}