#include <pl/core/ast/ast_node_bitfield.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>

#include <bit>

namespace pl::core::ast {

    ASTNodeBitfield::ASTNodeBitfield(const ASTNodeBitfield &other) : ASTNode(other), Attributable(other) {
        for (const auto &entry : other.getEntries())
            this->m_entries.emplace_back(entry->clone());
    }

    [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &ASTNodeBitfield::getEntries() const {
        return this->m_entries;
    }

    void ASTNodeBitfield::addEntry(std::unique_ptr<ASTNode> &&entry) {
        this->m_entries.emplace_back(std::move(entry));
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeBitfield::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        auto position = evaluator->getBitwiseReadOffset();
        auto bitfieldPattern = std::make_shared<ptrn::PatternBitfield>(evaluator, position.byteOffset, position.bitOffset, 0, getLine());

        bitfieldPattern->setSection(evaluator->getSectionId());

        bool prevReversed = evaluator->isReadOrderReversed();
        bool reversedChanged = false;
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
                        case u32(BitfieldOrder::MostToLeastSignificant):
                            return BitfieldOrder::MostToLeastSignificant;
                        case u32(BitfieldOrder::LeastToMostSignificant):
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
                reversedChanged = true;
                wolv::util::unused(evaluator->getBitwiseReadOffsetAndIncrement(size));
                evaluator->setReadOrderReversed(shouldBeReversed);
            }

            fixedSize = size;
        }

        std::vector<std::shared_ptr<ptrn::Pattern>> fields;
        std::vector<std::shared_ptr<ptrn::Pattern>> potentialPatterns;

        evaluator->pushScope(bitfieldPattern, potentialPatterns);
        ON_SCOPE_EXIT {
                          evaluator->popScope();
                      };

        auto initialPosition = evaluator->getBitwiseReadOffset();
        auto setSize = [&](ASTNode *node) {
            auto endPosition = evaluator->getBitwiseReadOffset();
            auto startOffset = (initialPosition.byteOffset * 8) + initialPosition.bitOffset;
            auto endOffset = (endPosition.byteOffset * 8) + endPosition.bitOffset;
            auto totalBitSize = startOffset > endOffset ? startOffset - endOffset : endOffset - startOffset;
            if (fixedSize > 0) {
                if (totalBitSize > fixedSize)
                    err::E0005.throwError("Bitfield's fields exceeded the attribute-allotted size.", {}, node);
                totalBitSize = fixedSize;
            }
            bitfieldPattern->setBitSize(totalBitSize);
        };

        for (auto &entry : this->m_entries) {
            auto patterns = entry->createPatterns(evaluator);
            setSize(entry.get());
            std::move(patterns.begin(), patterns.end(), std::back_inserter(potentialPatterns));

            if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                break;

            if (!evaluator->getCurrentArrayIndex().has_value()) {
                if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Break) {
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                    break;
                } else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Continue) {
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                    potentialPatterns.clear();
                    break;
                }
            }
        }

        for (auto &pattern : potentialPatterns) {
            if (auto bitfieldMember = dynamic_cast<ptrn::PatternBitfieldMember*>(pattern.get()); bitfieldMember != nullptr) {
                bitfieldMember->setParentBitfield(bitfieldPattern.get());
                if (!bitfieldMember->isPadding())
                    fields.push_back(pattern);
            } else {
                fields.push_back(pattern);
            }
        }

        bitfieldPattern->setReversed(evaluator->isReadOrderReversed());
        if (reversedChanged)
            evaluator->setBitwiseReadOffset(initialPosition);
        bitfieldPattern->setFields(fields);

        applyTypeAttributes(evaluator, this, bitfieldPattern);

        evaluator->setReadOrderReversed(prevReversed);

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(bitfieldPattern));
    }

}