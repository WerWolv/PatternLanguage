#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_pointer.hpp>

namespace pl::core::ast {

    class ASTNodePointerVariableDecl : public ASTNode,
                                       public Attributable {
    public:
        ASTNodePointerVariableDecl(std::string name, std::shared_ptr<ASTNode> type, std::shared_ptr<ASTNodeTypeDecl> sizeType, std::unique_ptr<ASTNode> &&placementOffset = nullptr, std::unique_ptr<ASTNode> &&placementSection = nullptr)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_sizeType(std::move(sizeType)), m_placementOffset(std::move(placementOffset)), m_placementSection(std::move(placementSection)) { }

        ASTNodePointerVariableDecl(const ASTNodePointerVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name     = other.m_name;
            this->m_type = std::shared_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));
            this->m_sizeType = other.m_sizeType;

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            if (other.m_placementSection != nullptr)
                this->m_placementSection = other.m_placementSection->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodePointerVariableDecl(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNode> &getType() const { return this->m_type; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNodeTypeDecl> &getSizeType() const { return this->m_sizeType; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto startOffset = evaluator->dataOffset();

            auto scopeGuard = SCOPE_GUARD {
                evaluator->popSectionId();
            };

            if (this->m_placementSection != nullptr) {
                const auto node = this->m_placementSection->evaluate(evaluator);
                const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());
                if (id == nullptr)
                    err::E0010.throwError("Cannot use void expression as section identifier.", {}, this);

                evaluator->pushSectionId(id->getValue().toUnsigned());
            } else {
                scopeGuard.release();
            }

            if (this->m_placementOffset != nullptr) {
                const auto node   = this->m_placementOffset->evaluate(evaluator);
                const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());
                if (offset == nullptr)
                    err::E0010.throwError("Cannot use void expression as placement offset.", {}, this);

                evaluator->dataOffset() = std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [this](ptrn::Pattern *) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [](auto &&offset) -> u64 { return u64(offset); }
                }, offset->getValue());
            }

            auto pointerStartOffset = evaluator->dataOffset();

            auto sizePatterns = this->m_sizeType->createPatterns(evaluator);
            if (sizePatterns.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this);

            auto &sizePattern = sizePatterns.front();

            auto pattern = std::make_shared<ptrn::PatternPointer>(evaluator, pointerStartOffset, sizePattern->getSize());
            pattern->setVariableName(this->m_name);
            pattern->setPointerTypePattern(std::move(sizePattern));

            auto pointerEndOffset = evaluator->dataOffset();

            {
                i128 pointerAddress = pattern->getValue().toSigned();

                evaluator->dataOffset() = pointerStartOffset;

                pattern->setPointedAtAddress(pointerAddress);
                applyVariableAttributes(evaluator, this, pattern);

                evaluator->dataOffset() = pattern->getPointedAtAddress();

                auto pointedAtPatterns = this->m_type->createPatterns(evaluator);
                if (pointedAtPatterns.empty())
                    err::E0005.throwError("'auto' can only be used with parameters.", { }, this);

                auto &pointedAtPattern = pointedAtPatterns.front();

                pattern->setPointedAtPattern(std::move(pointedAtPattern));
                pattern->setSection(evaluator->getSectionId());
            }

            if (this->m_placementSection != nullptr)
                pattern->setSection(evaluator->getSectionId());


            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            } else {
                evaluator->dataOffset() = pointerEndOffset;
            }

            if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId) {
                evaluator->dataOffset() = startOffset;
                this->execute(evaluator);
                return { };
            } else {
                return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
            }
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNode> m_type;
        std::shared_ptr<ASTNodeTypeDecl> m_sizeType;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;
    };

}
