#include <pl/core/ast/ast_node_enum.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_enum.hpp>

namespace pl::core::ast {

    ASTNodeEnum::ASTNodeEnum(std::unique_ptr<ASTNode> &&underlyingType) : ASTNode(), m_underlyingType(std::move(underlyingType)) { }

    ASTNodeEnum::ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other), Attributable(other) {
        for (const auto &[name, expr] : other.getEntries()) {
            auto &[min, max] = expr;
            this->m_entries[name] = { min->clone(), max == nullptr ? nullptr : max->clone() };
        }
        this->m_underlyingType = other.m_underlyingType->clone();

        this->m_cachedEnumValues = other.m_cachedEnumValues;
    }

    const std::vector<ptrn::PatternEnum::EnumValue>& ASTNodeEnum::getEnumValues(Evaluator *evaluator) const {
        if (m_cachedEnumValues.empty()) {
            for (const auto &[name, expr] : this->m_entries) {
                auto &[min, max] = expr;

                const auto minNode = min->evaluate(evaluator);
                const auto maxNode = max == nullptr ? minNode->clone() : max->evaluate(evaluator);

                const auto minLiteral = dynamic_cast<ASTNodeLiteral *>(minNode.get());
                const auto maxLiteral = dynamic_cast<ASTNodeLiteral *>(maxNode.get());

                if (minLiteral == nullptr || maxLiteral == nullptr)
                    err::E0010.throwError("Cannot use void expression as enum value.", {}, this->getLocation());

                // Check that the enum values can be converted to integers
                (void)minLiteral->getValue().toUnsigned();
                (void)maxLiteral->getValue().toUnsigned();

                m_cachedEnumValues.push_back(ptrn::PatternEnum::EnumValue{
                        minLiteral->getValue(),
                        maxLiteral->getValue(),
                        name
                });
            }
        }

        return m_cachedEnumValues;
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

        pattern->setEnumValues(getEnumValues(evaluator));

        pattern->setSize(underlying->getSize());
        pattern->setEndian(underlying->getEndian());

        applyTypeAttributes(evaluator, this, pattern);

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

    std::unique_ptr<ASTNode> ASTNodeEnum::evaluate(Evaluator *evaluator) const {
        wolv::util::unused(this->getEnumValues(evaluator));

        return this->clone();
    }


}