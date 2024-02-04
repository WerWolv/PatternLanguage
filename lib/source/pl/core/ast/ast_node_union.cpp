#include <pl/core/ast/ast_node_union.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/patterns/pattern_union.hpp>

namespace pl::core::ast {

    ASTNodeUnion::ASTNodeUnion(const ASTNodeUnion &other) : ASTNode(other), Attributable(other) {
        for (const auto &otherMember : other.getMembers())
            this->m_members.push_back(otherMember->clone());
    }

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeUnion::createPatterns(Evaluator *evaluator) const {
        evaluator->updateRuntime(this);

        evaluator->alignToByte();
        auto pattern = std::make_shared<ptrn::PatternUnion>(evaluator, evaluator->getReadOffset(), 0, getLocation().line);

        size_t size = 0;
        std::vector<std::shared_ptr<ptrn::Pattern>> memberPatterns;
        u64 startOffset = evaluator->getReadOffset();

        pattern->setSection(evaluator->getSectionId());

        evaluator->pushScope(pattern, memberPatterns);
        ON_SCOPE_EXIT {
            evaluator->popScope();
        };

        for (auto &member : this->m_members) {
            evaluator->setReadOffset(startOffset);

            for (auto &memberPattern : member->createPatterns(evaluator)) {
                size = std::max(memberPattern->getSize(), size);
                memberPattern->setSection(evaluator->getSectionId());
                memberPatterns.push_back(std::move(memberPattern));
            }
            pattern->setSize(size);

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

        evaluator->setReadOffset(startOffset + size);
        if (evaluator->isReadOrderReversed())
            pattern->setAbsoluteOffset(evaluator->getReadOffset());
        pattern->setMembers(memberPatterns);

        applyTypeAttributes(evaluator, this, pattern);

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

}