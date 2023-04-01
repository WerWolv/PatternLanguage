#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::core::ast {

    class ASTNodeBitfield : public ASTNode,
                            public Attributable {
    public:
        enum class BitfieldOrder {
            MostToLeastSignificant = 0,
            LeastToMostSignificant = 1,
        };

        ASTNodeBitfield() : ASTNode() { }

        ASTNodeBitfield(const ASTNodeBitfield &other) : ASTNode(other), Attributable(other) {
            for (const auto &entry : other.getEntries())
                this->m_entries.emplace_back(entry->clone());

            this->m_isNested = other.m_isNested;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfield(*this));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getEntries() const { return this->m_entries; }
        void addEntry(std::unique_ptr<ASTNode> &&entry) { this->m_entries.emplace_back(std::move(entry)); }

        void setNested() {
            this->m_isNested = true;
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto bitfieldPattern = std::make_shared<ptrn::PatternBitfield>(evaluator, evaluator->dataOffset(), evaluator->getBitfieldBitOffset(), 0);

            bitfieldPattern->setSection(evaluator->getSectionId());

            bool prevReversed = evaluator->isBitfieldReversed();
            bool didReverse = false;
            u128 fixedSize = 0;

            {
                auto *badAttribute = [&]() {
                    if (auto *ltrAttribute = this->getAttributeByName("left_to_right"); ltrAttribute != nullptr)
                        return ltrAttribute;
                    return this->getAttributeByName("right_to_left");
                }();
                if (badAttribute != nullptr)
                    err::E0008.throwError(fmt::format("Attribute '{}' is no longer supported.", badAttribute->getAttribute()), {}, badAttribute);
            }

            auto *orderAttribute = this->getAttributeByName("bitfield_order");
            if (orderAttribute != nullptr) {
                auto &arguments = orderAttribute->getArguments();
                if (arguments.size() != 2)
                    err::E0008.throwError(fmt::format("Attribute 'bitfield_order' expected 2 parameters, received {}.", arguments.size()), {}, orderAttribute);

                auto directionNode = arguments[0]->evaluate(evaluator);
                auto sizeNode = arguments[1]->evaluate(evaluator);

                BitfieldOrder order = [&]() {
                    if (auto *literalNode = dynamic_cast<ASTNodeLiteral *>(directionNode.get()); literalNode != nullptr) {
                        auto value = literalNode->getValue().toUnsigned();
                        switch (value) {
                        case std::to_underlying(BitfieldOrder::MostToLeastSignificant):
                            return BitfieldOrder::MostToLeastSignificant;
                        case std::to_underlying(BitfieldOrder::LeastToMostSignificant):
                            return BitfieldOrder::LeastToMostSignificant;
                        default:
                            err::E0008.throwError(fmt::format("Invalid BitfieldOrder value {}.", value), {}, arguments[0].get());
                        }
                    }
                    err::E0008.throwError("The 'direction' parameter for attribute 'bitfield_order' must not be void.", {}, arguments[0].get());
                }();
                u128 size = [&]() {
                    if (auto *literalNode = dynamic_cast<ASTNodeLiteral *>(sizeNode.get()); literalNode != nullptr) {
                        auto value = literalNode->getValue().toUnsigned();
                        if (value == 0)
                            err::E0008.throwError("Fixed size of a bitfield must be greater than zero.", {}, arguments[1].get());
                        return value;
                    }
                    err::E0008.throwError("The 'size' parameter for attribute 'bitfield_order' must not be void.", {}, arguments[1].get());
                }();

                bool shouldBeReversed = (order == BitfieldOrder::MostToLeastSignificant && bitfieldPattern->getEndian() == std::endian::little)
                    || (order == BitfieldOrder::LeastToMostSignificant && bitfieldPattern->getEndian() == std::endian::big);
                if (prevReversed != shouldBeReversed) {
                    didReverse = true;
                    evaluator->incrementBitfieldBitOffset(shouldBeReversed ? size : -size);
                    evaluator->setBitfieldReversed(shouldBeReversed);
                }

                fixedSize = size;
            }

            std::vector<std::shared_ptr<ptrn::Pattern>> fields;
            std::vector<std::shared_ptr<ptrn::Pattern>> potentialPatterns;

            evaluator->pushScope(bitfieldPattern, potentialPatterns);
            ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            auto initialByteOffset = evaluator->dataOffset();
            auto initialBitOffset = evaluator->getBitfieldBitOffset();

            for (auto &entry : this->m_entries) {
                auto patterns = entry->createPatterns(evaluator);
                std::move(patterns.begin(), patterns.end(), std::back_inserter(potentialPatterns));

                if (!evaluator->getCurrentArrayIndex().has_value()) {
                    if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                        break;
                    else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Break) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        break;
                    } else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Continue) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        potentialPatterns.clear();
                        break;
                    }
                }
            }

            auto startOffset = (initialByteOffset * 8) + initialBitOffset;
            auto endOffset = (evaluator->dataOffset() * 8) + evaluator->getBitfieldBitOffset();
            auto totalBitSize = startOffset > endOffset ? startOffset - endOffset : endOffset - startOffset;
            if (fixedSize > 0) {
                if (totalBitSize > fixedSize)
                    err::E0005.throwError("Bitfield's fields exceeded the attribute-allotted size.", {}, this);
                if (didReverse)
                    evaluator->incrementBitfieldBitOffset(totalBitSize);
                totalBitSize = fixedSize;
            }
            bitfieldPattern->setBitSize(totalBitSize);

          for (auto &pattern : potentialPatterns) {
                if (auto bitfieldMember = dynamic_cast<ptrn::PatternBitfieldMember*>(pattern.get()); bitfieldMember != nullptr) {
                    bitfieldMember->setParentBitfield(bitfieldPattern.get());
                    if (!bitfieldMember->isPadding())
                        fields.push_back(pattern);
                } else {
                    fields.push_back(pattern);
                }
            }

            bitfieldPattern->setReversed(evaluator->isBitfieldReversed());
            bitfieldPattern->setFields(fields);

            applyTypeAttributes(evaluator, this, bitfieldPattern);

            if (!this->m_isNested)
                evaluator->resetBitfieldBitOffset();
            evaluator->setBitfieldReversed(prevReversed);

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(bitfieldPattern));
        }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_entries;

        bool m_isNested = false;
    };

}