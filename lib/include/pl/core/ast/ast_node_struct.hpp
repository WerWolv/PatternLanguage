#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_struct.hpp>

namespace pl::core::ast {

    class ASTNodeStruct : public ASTNode,
                          public Attributable {
    public:
        ASTNodeStruct() : ASTNode() { }

        ASTNodeStruct(const ASTNodeStruct &other) : ASTNode(other), Attributable(other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
            for (const auto &otherInheritance : other.getInheritance())
                this->m_inheritance.push_back(otherInheritance->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeStruct(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            evaluator->alignToByte();
            auto pattern = std::make_shared<ptrn::PatternStruct>(evaluator, evaluator->getReadOffset(), 0);

            auto startOffset = evaluator->getReadOffset();
            std::vector<std::shared_ptr<ptrn::Pattern>> memberPatterns;

            pattern->setSection(evaluator->getSectionId());

            evaluator->pushScope(pattern, memberPatterns);
            ON_SCOPE_EXIT {
                evaluator->popScope();
                evaluator->alignToByte();
            };

            auto setSize = [&]() {
                auto currentOffset = evaluator->getReadOffset();
                pattern->setSize(currentOffset > startOffset ? currentOffset - startOffset : startOffset - currentOffset);
            };

            for (auto &inheritance : this->m_inheritance) {
                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    break;

                auto inheritancePatterns = inheritance->createPatterns(evaluator);
                auto &inheritancePattern = inheritancePatterns.front();

                if (auto structPattern = dynamic_cast<ptrn::PatternStruct *>(inheritancePattern.get())) {
                    for (auto &member : structPattern->getEntries()) {
                        memberPatterns.push_back(member);
                    }
                    setSize();
                }
            }

            for (auto &member : this->m_members) {
                evaluator->alignToByte();
                for (auto &memberPattern : member->createPatterns(evaluator)) {
                    memberPattern->setSection(evaluator->getSectionId());
                    memberPatterns.push_back(std::move(memberPattern));
                }
                setSize();

                if (!evaluator->getCurrentArrayIndex().has_value()) {
                    if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                        break;
                    else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Break) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        break;
                    } else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Continue) {
                        evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        memberPatterns.clear();
                        evaluator->setReadOffset(startOffset);
                        break;
                    }
                }
            }

            pattern->setMembers(memberPatterns);

            if (evaluator->readOrderIsReversed())
                pattern->setAbsoluteOffset(evaluator->getReadOffset());

            applyTypeAttributes(evaluator, this, pattern);

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getMembers() const { return this->m_members; }
        void addMember(std::shared_ptr<ASTNode> &&node) { this->m_members.push_back(std::move(node)); }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getInheritance() const { return this->m_inheritance; }
        void addInheritance(std::shared_ptr<ASTNode> &&node) { this->m_inheritance.push_back(std::move(node)); }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_members;
        std::vector<std::shared_ptr<ASTNode>> m_inheritance;
    };

}