#include <pl/core/ast/ast_node_enum.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_enum.hpp>

namespace pl::core::ast {

    ASTNodeEnum::ASTNodeEnum(std::unique_ptr<ASTNode> &&underlyingType) : m_underlyingType(std::move(underlyingType)) { }

    ASTNodeEnum::ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other), Attributable(other) {
        for (const auto &[name, expr] : other.getEntries()) {
            auto &[min, max] = expr;
            this->m_entries[name] = { min->clone(), max == nullptr ? nullptr : max->clone() };
        }
        this->m_underlyingType = other.m_underlyingType->clone();

        this->m_cachedEnumValues = other.m_cachedEnumValues;
    }

    [[nodiscard]] const ptrn::PatternEnum::EnumValue& ASTNodeEnum::getEnumValue(Evaluator *evaluator, const std::string &name) const {
        if (!m_cachedEnumValues.contains(name)) {
            auto it = this->m_entries.find(name);
            if (it == this->m_entries.end())
                err::E0010.throwError(fmt::format("Cannot find enum value with name '{}'.", name), {}, this->getLocation());

            auto &[min, max] = it->second;

            const auto minNode = min->evaluate(evaluator);
            const auto maxNode = max == nullptr ? minNode->clone() : max->evaluate(evaluator);

            const auto minLiteral = dynamic_cast<ASTNodeLiteral *>(minNode.get());
            const auto maxLiteral = dynamic_cast<ASTNodeLiteral *>(maxNode.get());

            if (minLiteral == nullptr || maxLiteral == nullptr)
                err::E0010.throwError("Cannot use void expression as enum value.", {}, this->getLocation());

            // Check that the enum values can be converted to integers
            (void)minLiteral->getValue().toUnsigned();
            (void)maxLiteral->getValue().toUnsigned();

            m_cachedEnumValues[name] = ptrn::PatternEnum::EnumValue {
                .min = minLiteral->getValue(),
                .max = maxLiteral->getValue()
            };
        }

        return m_cachedEnumValues[name];
    }

    [[nodiscard]] const std::map<std::string, ptrn::PatternEnum::EnumValue>& ASTNodeEnum::getEnumValues(Evaluator *evaluator) const {
        if (m_cachedEnumValues.size() != m_entries.size()) {
            for (const auto &[name, values] : m_entries) {
                (void)getEnumValue(evaluator, name);
            }
        }

        return m_cachedEnumValues;
    }


    void ASTNodeEnum::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        evaluator->alignToByte();

        std::vector<std::shared_ptr<ptrn::Pattern>> underlyingTypePatterns;
        this->m_underlyingType->createPatterns(evaluator, underlyingTypePatterns);
        if (underlyingTypePatterns.empty())
            err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());
        auto &underlying = underlyingTypePatterns.front();

        auto pattern = construct_shared_object<PatternEnum>(evaluator, underlying->getOffset(), 0, getLocation().line);

        pattern->setSection(evaluator->getSectionId());

        pattern->setEnumValues(getEnumValues(evaluator));

        pattern->setSize(underlying->getSize());
        pattern->setEndian(underlying->getEndian());

        applyTypeAttributes(evaluator, this, pattern);

        resultPatterns = hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

    std::unique_ptr<ASTNode> ASTNodeEnum::evaluate(Evaluator *) const {
        return this->clone();
    }


}