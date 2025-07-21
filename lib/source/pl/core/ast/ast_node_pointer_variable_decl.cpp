#include <pl/core/ast/ast_node_pointer_variable_decl.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_pointer.hpp>

namespace pl::core::ast {

    ASTNodePointerVariableDecl::ASTNodePointerVariableDecl(std::string name, std::shared_ptr<ASTNode> type, std::shared_ptr<ASTNodeTypeDecl> sizeType, std::unique_ptr<ASTNode> &&placementOffset, std::unique_ptr<ASTNode> &&placementSection)
    : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_sizeType(std::move(sizeType)), m_placementOffset(std::move(placementOffset)), m_placementSection(std::move(placementSection)) { }

    ASTNodePointerVariableDecl::ASTNodePointerVariableDecl(const ASTNodePointerVariableDecl &other) : ASTNode(other), Attributable(other) {
        this->m_name     = other.m_name;
        this->m_type     = other.m_type->clone();
        this->m_sizeType = std::shared_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_sizeType->clone().release()));

        if (other.m_placementOffset != nullptr)
            this->m_placementOffset = other.m_placementOffset->clone();
        if (other.m_placementSection != nullptr)
            this->m_placementSection = other.m_placementSection->clone();
    }

    void ASTNodePointerVariableDecl::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();

        auto scopeGuard = SCOPE_GUARD {
            evaluator->popSectionId();
        };

        if (this->m_placementSection != nullptr) {
            const auto node = this->m_placementSection->evaluate(evaluator);
            const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (id == nullptr)
                err::E0010.throwError("Cannot use void expression as section identifier.", {}, this->getLocation());

            evaluator->pushSectionId(u64(id->getValue().toUnsigned()));
        } else {
            scopeGuard.release();
        }

        if (this->m_placementOffset != nullptr) {
            const auto node   = this->m_placementOffset->evaluate(evaluator);
            const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (offset == nullptr)
                err::E0010.throwError("Cannot use void expression as placement offset.", {}, this->getLocation());

            evaluator->setReadOffset(std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this->getLocation()); },
                    [this](const std::shared_ptr<ptrn::Pattern> &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this->getLocation()); },
                    [](auto &&offset) -> u64 { return u64(offset); }
            }, offset->getValue()));
        }

        auto pointerStartOffset = evaluator->getReadOffset();

        std::vector<std::shared_ptr<ptrn::Pattern>> sizePatterns;
        this->m_sizeType->createPatterns(evaluator, sizePatterns);
        if (sizePatterns.empty())
            err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());

        auto &sizePattern = sizePatterns.front();
        sizePattern->setSection(evaluator->getSectionId());

        auto pattern = std::make_shared<ptrn::PatternPointer>(evaluator, pointerStartOffset, sizePattern->getSize(), getLocation().line);
        pattern->setVariableName(this->m_name, this->getLocation());
        pattern->setPointerTypePattern(std::move(sizePattern));

        ON_SCOPE_EXIT {
            resultPatterns = hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
        };

        auto pointerEndOffset = evaluator->getBitwiseReadOffset();

        {
            i128 pointerAddress = pattern->getValue().toSigned();

            evaluator->setReadOffset(pointerStartOffset);

            pattern->setPointedAtAddress(pointerAddress);
            applyVariableAttributes(evaluator, this, pattern);

            evaluator->setReadOffset(u64(pattern->getPointedAtAddress()));

            std::vector<std::shared_ptr<ptrn::Pattern>> pointedAtPatterns;
            ON_SCOPE_EXIT {
                auto &pointedAtPattern = pointedAtPatterns.front();

                pattern->setPointedAtPattern(std::move(pointedAtPattern));
                pattern->setSection(evaluator->getSectionId());
            };

            this->m_type->createPatterns(evaluator, pointedAtPatterns);
            if (pointedAtPatterns.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());
        }

        if (this->m_placementSection != nullptr)
            pattern->setSection(evaluator->getSectionId());


        if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
            evaluator->setBitwiseReadOffset(startOffset);
        } else {
            evaluator->setBitwiseReadOffset(pointerEndOffset);
        }

        if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId) {
            evaluator->setBitwiseReadOffset(startOffset);
            this->execute(evaluator);
            resultPatterns.clear();
        } else {
            if (this->m_placementSection != nullptr && !evaluator->isGlobalScope()) {
                evaluator->addPattern(std::move(pattern));
                resultPatterns.clear();
            }
        }
    }

}
