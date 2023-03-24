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

            if (this->hasAttribute("left_to_right", false))
                bitfieldPattern->setEndian(std::endian::big);
            else if (this->hasAttribute("right_to_left", false))
                bitfieldPattern->setEndian(std::endian::little);
            else if (evaluator->getBitfieldOrder().has_value()) {
                switch (evaluator->getBitfieldOrder().value()) {
                case BitfieldOrder::LeftToRight:
                    bitfieldPattern->setEndian(std::endian::big);
                    break;
                case BitfieldOrder::RightToLeft:
                    bitfieldPattern->setEndian(std::endian::little);
                    break;
                }
            }

            std::vector<std::shared_ptr<ptrn::Pattern>> fields;
            std::vector<std::shared_ptr<ptrn::Pattern>> potentialPatterns;

            auto prevDefaultEndian = evaluator->getDefaultEndian();
            evaluator->pushScope(bitfieldPattern, potentialPatterns);
            evaluator->setDefaultEndian(bitfieldPattern->getEndian());
            ON_SCOPE_EXIT {
                evaluator->setDefaultEndian(prevDefaultEndian);
                evaluator->popScope();
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

            if (potentialPatterns.size() > 0) {
                auto lastMemberIter = std::find_if(potentialPatterns.rbegin(), potentialPatterns.rend(), [](auto& pattern) {
                    return dynamic_cast<ptrn::PatternBitfieldMember*>(pattern.get()) != nullptr;
                });
                if (lastMemberIter != potentialPatterns.rend()) {
                    auto *lastMember = static_cast<ptrn::PatternBitfieldMember*>(lastMemberIter->get());
                    auto totalBitSize = (lastMember->getTotalBitOffset() - bitfieldPattern->getTotalBitOffset()) + lastMember->getBitSize();
                    bitfieldPattern->setBitSize(totalBitSize);
                }

                for (auto &pattern : potentialPatterns) {
                    if (auto bitfieldMember = dynamic_cast<ptrn::PatternBitfieldMember*>(pattern.get()); bitfieldMember != nullptr) {
                        bitfieldMember->setParentBitfield(bitfieldPattern.get());
                        if (!bitfieldMember->isPadding())
                            fields.push_back(std::move(pattern));
                    } else {
                        fields.push_back(std::move(pattern));
                    }
                }
            }

            bitfieldPattern->setFields(fields);

            applyTypeAttributes(evaluator, this, bitfieldPattern);

            if (!this->m_isNested)
                evaluator->resetBitfieldBitOffset();

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(bitfieldPattern));
        }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_entries;

        bool m_isNested = false;
    };

}