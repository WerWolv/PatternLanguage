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
            this->m_type     = other.m_type;
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
            auto startOffset = evaluator->dataOffset();

            auto scopeGuard = PL_SCOPE_GUARD {
                evaluator->popSectionId();
            };

            if (this->m_placementSection != nullptr) {
                const auto node = this->m_placementSection->evaluate(evaluator);
                const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());

                evaluator->pushSectionId(Token::literalToUnsigned(id->getValue()));
            } else {
                scopeGuard.release();
            }

            if (this->m_placementOffset != nullptr) {
                const auto node   = this->m_placementOffset->evaluate(evaluator);
                const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());

                evaluator->dataOffset() = std::visit(hlp::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [this](ptrn::Pattern *) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [](auto &&offset) -> u64 { return u64(offset); }
                }, offset->getValue());
            }

            auto pointerStartOffset = evaluator->dataOffset();

            auto sizePatterns = this->m_sizeType->createPatterns(evaluator);
            auto &sizePattern = sizePatterns.front();

            auto pattern = std::make_unique<ptrn::PatternPointer>(evaluator, pointerStartOffset, sizePattern->getSize());
            pattern->setVariableName(this->m_name);
            pattern->setPointerTypePattern(std::move(sizePattern));
            pattern->setSection(evaluator->getSectionId());

            auto pointerEndOffset = evaluator->dataOffset();

            {
                i128 pointerAddress = core::Token::literalToSigned(pattern->getValue());

                evaluator->dataOffset() = pointerStartOffset;

                pattern->setPointedAtAddress(pointerAddress);
                applyVariableAttributes(evaluator, this, pattern.get());

                evaluator->dataOffset() = pattern->getPointedAtAddress();

                auto pointedAtPatterns = this->m_type->createPatterns(evaluator);
                auto &pointedAtPattern = pointedAtPatterns.front();
                pattern->setSection(evaluator->getSectionId());

                pattern->setPointedAtPattern(std::move(pointedAtPattern));
            }

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            } else {
                evaluator->dataOffset() = pointerEndOffset;
            }

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNode> m_type;
        std::shared_ptr<ASTNodeTypeDecl> m_sizeType;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;
    };

}
