#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_union.hpp>

namespace pl::core::ast {

    class ASTNodeUnion : public ASTNode,
                         public Attributable {
    public:
        ASTNodeUnion() : ASTNode() { }

        ASTNodeUnion(const ASTNodeUnion &other) : ASTNode(other), Attributable(other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeUnion(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto pattern = std::make_unique<ptrn::PatternUnion>(evaluator, evaluator->dataOffset(), 0);

            size_t size = 0;
            std::vector<std::shared_ptr<ptrn::Pattern>> memberPatterns;
            u64 startOffset = evaluator->dataOffset();

            evaluator->pushScope(pattern.get(), memberPatterns);
            PL_ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            for (auto &member : this->m_members) {
                for (auto &memberPattern : member->createPatterns(evaluator)) {
                    memberPattern->setOffset(startOffset);
                    size = std::max(memberPattern->getSize(), size);

                    memberPatterns.push_back(std::move(memberPattern));
                }
            }

            evaluator->dataOffset() = startOffset + size;
            pattern->setMembers(std::move(memberPatterns));
            pattern->setSize(size);

            applyTypeAttributes(evaluator, this, pattern.get());

            return hlp::moveToVector<std::unique_ptr<ptrn::Pattern>>(std::move(pattern));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getMembers() const { return this->m_members; }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_members;
    };

}