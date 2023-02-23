#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::core::ast {

    class ASTNodeBitfield : public ASTNode,
                            public Attributable {
    public:
        ASTNodeBitfield() : ASTNode() { }

        ASTNodeBitfield(const ASTNodeBitfield &other) : ASTNode(other), Attributable(other) {
            for (const auto &entry : other.getEntries())
                this->m_entries.emplace_back(entry->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfield(*this));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getEntries() const { return this->m_entries; }
        void addEntry(std::unique_ptr<ASTNode> &&entry) { this->m_entries.emplace_back(std::move(entry)); }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto bitfieldPattern = std::make_shared<ptrn::PatternBitfield>(evaluator, evaluator->dataOffset(), 0);
            if (this->hasAttribute("left_to_right", false))
                bitfieldPattern->setEndian(std::endian::big);
            else if (this->hasAttribute("right_to_left", false))
                bitfieldPattern->setEndian(std::endian::little);
            else {
                switch (evaluator->getBitfieldOrder()) {
                case BitfieldOrder::LeftToRight:
                    bitfieldPattern->setEndian(std::endian::big);
                    break;
                case BitfieldOrder::RightToLeft:
                    bitfieldPattern->setEndian(std::endian::little);
                    break;
                }
            }

            size_t bitOffset = 0x00;

            std::vector<std::shared_ptr<ptrn::Pattern>> fields;
            std::vector<std::shared_ptr<ptrn::Pattern>> potentialPatterns;

            auto prevBitfieldFieldAddedCallback = evaluator->getBitfieldFieldAddedCallback();
            evaluator->pushScope(bitfieldPattern, potentialPatterns);
            evaluator->setBitfieldFieldAddedCallback([&](const ast::ASTNodeBitfieldField& node, std::shared_ptr<ptrn::PatternBitfieldField> field) {
                if (field->getSize() == 0) {
                    field->setEndian(bitfieldPattern->getEndian());
                    field->setBitOffset(bitOffset);
                    field->setSection(evaluator->getSectionId());
                    bitOffset += field->getBitSize();
                }

                bitfieldPattern->setSize((bitOffset + 7) / 8);

                applyVariableAttributes(evaluator, &node, field);
            });
            ON_SCOPE_EXIT {
                evaluator->popScope();
                evaluator->setBitfieldFieldAddedCallback(move(prevBitfieldFieldAddedCallback));
            };

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

            for (auto &pattern : potentialPatterns) {
                if (auto bitfieldField = dynamic_cast<ptrn::PatternBitfieldField*>(pattern.get()); bitfieldField != nullptr) {
                    bitfieldField->setBitfield(bitfieldPattern.get());
                    bitfieldField->setBitfieldBitSize(bitOffset);
                    if (!bitfieldField->isPadding())
                        fields.push_back(std::move(pattern));
                }
            }

            bitfieldPattern->setSize((bitOffset + 7) / 8);
            bitfieldPattern->setFields(fields);

            evaluator->dataOffset() += bitfieldPattern->getSize();

            applyTypeAttributes(evaluator, this, bitfieldPattern);

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(bitfieldPattern));
        }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_entries;
    };

}