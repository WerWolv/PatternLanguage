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
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

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
                const auto &varName = memberPattern->getVariableName();
                if (varName.starts_with("$") && varName.ends_with("$"))
                    continue;

                for (auto &existingPattern : memberPatterns) {
                    if (existingPattern->getVariableName() == varName) {
                        err::E0003.throwError(fmt::format("Redeclaration of identifier '{}'.", existingPattern->getVariableName()), "", member->getLocation());
                    }
                }

                size = std::max(memberPattern->getSize(), size);
                memberPattern->setSection(evaluator->getSectionId());
                memberPatterns.push_back(std::move(memberPattern));
            }
            pattern->setSize(size);

            if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                break;

            if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Break) {
                break;
            } else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Continue) {
                memberPatterns.clear();
                evaluator->setReadOffset(startOffset);
                break;
            }
        }

        evaluator->setReadOffset(startOffset + size);
        if (evaluator->isReadOrderReversed())
            pattern->setAbsoluteOffset(evaluator->getReadOffset());
        pattern->setEntries(memberPatterns);

        applyTypeAttributes(evaluator, this, pattern);

        return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
    }

}