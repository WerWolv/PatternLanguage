#include <pl/core/ast/ast_node_struct.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/patterns/pattern_struct.hpp>

namespace pl::core::ast {

    ASTNodeStruct::ASTNodeStruct(const ASTNodeStruct &other) : ASTNode(other), Attributable(other) {
        for (const auto &otherMember : other.getMembers())
            this->m_members.push_back(otherMember->clone());
        for (const auto &otherInheritance : other.getInheritance())
            this->m_inheritance.push_back(otherInheritance->clone());
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeStruct::createPatterns(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        evaluator->alignToByte();
        auto pattern = std::make_shared<ptrn::PatternStruct>(evaluator, evaluator->getReadOffset(), 0, getLocation().line);

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

            if (const auto &inheritedAttributes = inheritancePattern->getAttributes(); inheritedAttributes != nullptr) {
                for (const auto &[name, value] : *inheritedAttributes) {
                    pattern->addAttribute(name, value);
                }
            }
        }

        for (auto &member : this->m_members) {
            evaluator->alignToByte();
            for (auto &memberPattern : member->createPatterns(evaluator)) {
                memberPattern->setSection(evaluator->getSectionId());
                memberPatterns.push_back(std::move(memberPattern));
            }
            setSize();

            if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                break;

            if (!evaluator->getCurrentArrayIndex().has_value()) {
                if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Break) {
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

        if (evaluator->isReadOrderReversed())
            pattern->setAbsoluteOffset(evaluator->getReadOffset());

        applyTypeAttributes(evaluator, this, pattern);

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

}