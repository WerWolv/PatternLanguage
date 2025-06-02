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

    void ASTNodeStruct::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        evaluator->alignToByte();
        auto pattern = ptrn::PatternStruct::create(evaluator, evaluator->getReadOffset(), 0, getLocation().line);

        auto startOffset = evaluator->getReadOffset();
        std::vector<std::shared_ptr<ptrn::Pattern>> memberPatterns;

        pattern->setSection(evaluator->getSectionId());

        evaluator->pushScope(pattern, memberPatterns);
        ON_SCOPE_EXIT {
            pattern->setEntries(memberPatterns);

            if (evaluator->isReadOrderReversed())
                pattern->setAbsoluteOffset(evaluator->getReadOffset());

            applyTypeAttributes(evaluator, this, pattern);

            resultPatterns = hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));

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

            std::vector<std::shared_ptr<ptrn::Pattern>> inheritancePatterns;
            inheritance->createPatterns(evaluator, inheritancePatterns);
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
            std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
            member->createPatterns(evaluator, patterns);
            for (auto &memberPattern : patterns) {
                const auto &varName = memberPattern->getVariableName();
                if (varName.starts_with("$") && varName.ends_with("$"))
                    continue;

                for (auto &existingPattern : memberPatterns) {
                    if (existingPattern->getVariableName() == varName) {
                        err::E0003.throwError(fmt::format("Redeclaration of identifier '{}'.", existingPattern->getVariableName()), "", member->getLocation());
                    }
                }

                memberPattern->setSection(evaluator->getSectionId());
                memberPatterns.push_back(std::move(memberPattern));
            }
            setSize();

            if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Return)
                break;

            if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Break) {
                break;
            } else if (evaluator->getCurrentControlFlowStatement() == ControlFlowStatement::Continue) {
                memberPatterns.clear();
                break;
            }
        }
    }

}