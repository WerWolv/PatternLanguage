#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_enum.hpp>

namespace pl::core::ast {

    class ASTNodeEnum : public ASTNode,
                        public Attributable {
    public:
        explicit ASTNodeEnum(std::unique_ptr<ASTNode> &&underlyingType) : ASTNode(), m_underlyingType(std::move(underlyingType)) { }

        ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other), Attributable(other) {
            for (const auto &[name, expr] : other.getEntries()) {
                this->m_entries[name] = { expr.first->clone(), expr.second->clone() };
            }
            this->m_underlyingType = other.m_underlyingType->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeEnum(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto pattern = std::make_shared<ptrn::PatternEnum>(evaluator, evaluator->dataOffset(), 0);

            pattern->setSection(evaluator->getSectionId());

            std::vector<ptrn::PatternEnum::EnumValue> enumEntries;
            for (const auto &[name, expr] : this->m_entries) {
                auto &[min, max] = expr;

                const auto minNode = min->evaluate(evaluator);
                const auto maxNode = max->evaluate(evaluator);

                const auto minLiteral = dynamic_cast<ASTNodeLiteral *>(minNode.get());
                const auto maxLiteral = dynamic_cast<ASTNodeLiteral *>(maxNode.get());

                if (minLiteral == nullptr || maxLiteral == nullptr)
                    err::E0010.throwError("Cannot use void expression as enum value.", {}, this);

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

            const auto nodes = this->m_underlyingType->createPatterns(evaluator);
            if (nodes.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this);
            auto &underlying = nodes.front();

            pattern->setSize(underlying->getSize());
            pattern->setEndian(underlying->getEndian());

            applyTypeAttributes(evaluator, this, pattern);

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
        }

        [[nodiscard]] const std::map<std::string, std::pair<std::unique_ptr<ASTNode>, std::unique_ptr<ASTNode>>> &getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, std::unique_ptr<ASTNode> &&minExpr, std::unique_ptr<ASTNode> &&maxExpr) {
            this->m_entries[name] = { std::move(minExpr), std::move(maxExpr) };
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getUnderlyingType() { return this->m_underlyingType; }

    private:
        std::map<std::string, std::pair<std::unique_ptr<ASTNode>, std::unique_ptr<ASTNode>>> m_entries;
        std::unique_ptr<ASTNode> m_underlyingType;
    };

}