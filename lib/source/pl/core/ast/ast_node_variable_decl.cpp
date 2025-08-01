#include <pl/core/ast/ast_node_variable_decl.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/patterns/pattern_string.hpp>

namespace pl::core::ast {

    ASTNodeVariableDecl::ASTNodeVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&placementOffset, std::unique_ptr<ASTNode> &&placementSection, bool inVariable, bool outVariable, bool constant)
        : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_placementOffset(std::move(placementOffset)), m_placementSection(std::move(placementSection)), m_inVariable(inVariable), m_outVariable(outVariable), m_constant(constant) { }

    ASTNodeVariableDecl::ASTNodeVariableDecl(const ASTNodeVariableDecl &other) : ASTNode(other), Attributable(other) {
        this->m_name = other.m_name;
        this->m_type = std::shared_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));

        if (other.m_placementOffset != nullptr)
            this->m_placementOffset = other.m_placementOffset->clone();

        if (other.m_placementSection != nullptr)
            this->m_placementSection = other.m_placementSection->clone();

        this->m_inVariable  = other.m_inVariable;
        this->m_outVariable = other.m_outVariable;
        this->m_constant    = other.m_constant;
    }

    void ASTNodeVariableDecl::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();

        auto scopeGuard = SCOPE_GUARD {
            evaluator->popSectionId();
        };

        if (this->m_placementSection != nullptr) {
            const auto node = this->m_placementSection->evaluate(evaluator);
            const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (id == nullptr)
                err::E0002.throwError("Cannot use void expression as section identifier.", {}, this->getLocation());

            evaluator->pushSectionId((u64)id->getValue().toUnsigned());
        } else {
            scopeGuard.release();
        }

        if (this->m_placementOffset != nullptr) {
            const auto node   = this->m_placementOffset->evaluate(evaluator);
            const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (offset == nullptr)
                err::E0002.throwError("Void expression used in placement expression.", { }, this->getLocation());

            evaluator->setReadOffset(std::visit(wolv::util::overloaded {
                [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this->getLocation()); },
                [this](const std::shared_ptr<ptrn::Pattern> &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this->getLocation()); },
                [](auto &&offset) -> u64 { return u64(offset); } },
            offset->getValue()));

            if (evaluator->getReadOffset() < evaluator->getDataBaseAddress() || evaluator->getReadOffset() > evaluator->getDataBaseAddress() + evaluator->getDataSize())
                err::E0005.throwError(fmt::format("Cannot place variable '{}' at out of bounds address 0x{:08X}", this->m_name, evaluator->getReadOffset()), { }, this->getLocation());
        }

        if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId || evaluator->getSectionId() == ptrn::Pattern::HeapSectionId) {
            evaluator->setBitwiseReadOffset(startOffset);
            this->execute(evaluator);
        } else {
            std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
            ON_SCOPE_EXIT {
                if (!patterns.empty()) {
                    auto &pattern = patterns.front();
                    if (this->m_placementOffset != nullptr && dynamic_cast<ptrn::PatternString*>(pattern.get()) != nullptr)
                        err::E0005.throwError(fmt::format("Variables of type 'str' cannot be placed in memory.", this->m_name), { }, this->getLocation());

                    pattern->setVariableName(this->m_name, this->getLocation());

                    if (this->m_placementSection != nullptr)
                        pattern->setSection(evaluator->getSectionId());

                    applyVariableAttributes(evaluator, this, pattern);

                    if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                        evaluator->setBitwiseReadOffset(startOffset);
                    }

                    if (this->m_placementSection != nullptr && !evaluator->isGlobalScope()) {
                        evaluator->addPattern(std::move(pattern));
                    } else {
                        resultPatterns = hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
                    }
                }
            };

            this->m_type->createPatterns(evaluator, patterns);
            if (patterns.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this->getLocation());
        }
    }

    u128 ASTNodeVariableDecl::evaluatePlacementOffset(Evaluator *evaluator) const {
        const auto placementNode = this->m_placementOffset->evaluate(evaluator);
        const auto offsetLiteral = dynamic_cast<ASTNodeLiteral *>(placementNode.get());
        if (offsetLiteral == nullptr)
            err::E0002.throwError("Void expression used in placement expression.", { }, this->getLocation());

        return offsetLiteral->getValue().toUnsigned();
    }

    u64 ASTNodeVariableDecl::evaluatePlacementSection(Evaluator *evaluator) const {
        u64 section = 0;
        if (this->m_placementSection != nullptr) {
            const auto sectionNode = this->m_placementSection->evaluate(evaluator);
            const auto sectionLiteral = dynamic_cast<ASTNodeLiteral *>(sectionNode.get());
            if (sectionLiteral == nullptr)
                err::E0002.throwError("Cannot use void expression as section identifier.", {}, this->getLocation());

            auto value = sectionLiteral->getValue();
            section = u64(value.toUnsigned());
        }

        return section;
    }

    ASTNode::FunctionResult ASTNodeVariableDecl::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto startOffset = evaluator->getBitwiseReadOffset();

        // TODO: debug-menu: can get location here.
        evaluator->createVariable(this->getName(), this->getLocation(), this->getType().get(), { }, this->m_outVariable, false, false, this->m_constant);
        auto &variable = evaluator->getScope(0).scope->back();

        std::vector<std::shared_ptr<ptrn::Pattern>> initValues;
        if (this->m_placementOffset == nullptr) {
            evaluator->pushSectionId(ptrn::Pattern::InstantiationSectionId);
            this->getType()->createPatterns(evaluator, initValues);
            evaluator->popSectionId();
        } else {
            evaluator->pushSectionId(this->m_placementSection == nullptr ? ptrn::Pattern::MainSectionId : evaluator->getSectionId());

            auto currOffset = evaluator->getBitwiseReadOffset();
            ON_SCOPE_EXIT { evaluator->setBitwiseReadOffset(currOffset); };

            evaluator->setReadOffset(u64(this->evaluatePlacementOffset(evaluator)));
            this->getType()->createPatterns(evaluator, initValues);
            evaluator->popSectionId();
        }

        if (!initValues.empty()) {
            auto &initValue = initValues.front();
            if (variable->getSection() == ptrn::Pattern::HeapSectionId) {
                auto &heap = evaluator->getHeap();
                heap.emplace_back();
                heap.back().resize(initValue->getSize());

                initValue->setSection(ptrn::Pattern::HeapSectionId);
                initValue->setOffset(u64(heap.size() - 1) << 32);
            } else if (variable->getSection() == ptrn::Pattern::PatternLocalSectionId) {
                evaluator->changePatternSection(initValue.get(), ptrn::Pattern::PatternLocalSectionId);
                initValue->setOffset(0);
            }

            initValue->setTypeName(variable->getTypeName());
            evaluator->setVariable(variable, std::move(initValue));
            variable->setInitialized(false);
        }

        evaluator->setBitwiseReadOffset(startOffset);

        if (this->m_placementOffset != nullptr) {
            auto section = this->evaluatePlacementSection(evaluator);
            auto offset = this->evaluatePlacementOffset(evaluator);
            evaluator->setVariableAddress(this->getName(), u64(offset), section);
        }

        return std::nullopt;
    }
}