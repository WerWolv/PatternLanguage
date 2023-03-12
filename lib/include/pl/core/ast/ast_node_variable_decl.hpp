#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

namespace pl::core::ast {

    class ASTNodeVariableDecl : public ASTNode,
                                public Attributable {
    public:
        ASTNodeVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::unique_ptr<ASTNode> &&placementOffset = nullptr, std::unique_ptr<ASTNode> &&placementSection = nullptr, bool inVariable = false, bool outVariable = false, bool constant = false)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_placementOffset(std::move(placementOffset)), m_placementSection(std::move(placementSection)), m_inVariable(inVariable), m_outVariable(outVariable), m_constant(constant) { }

        ASTNodeVariableDecl(const ASTNodeVariableDecl &other) : ASTNode(other), Attributable(other) {
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

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeVariableDecl(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNodeTypeDecl> &getType() const { return this->m_type; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] constexpr bool isInVariable() const { return this->m_inVariable; }
        [[nodiscard]] constexpr bool isOutVariable() const { return this->m_outVariable; }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            u64 startOffset = evaluator->dataOffset();

            auto scopeGuard = SCOPE_GUARD {
                evaluator->popSectionId();
            };

            if (this->m_placementSection != nullptr) {
                const auto node = this->m_placementSection->evaluate(evaluator);
                const auto id = dynamic_cast<ASTNodeLiteral *>(node.get());
                if (id == nullptr)
                    err::E0002.throwError("Cannot use void expression as section identifier.", {}, this);

                evaluator->pushSectionId(id->getValue().toUnsigned());
            } else {
                scopeGuard.release();
            }

            if (this->m_placementOffset != nullptr) {
                const auto node   = this->m_placementOffset->evaluate(evaluator);
                const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());
                if (offset == nullptr)
                    err::E0002.throwError("Void expression used in placement expression.", { }, this);

                evaluator->dataOffset() = std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [this](ptrn::Pattern *) -> u64 { err::E0005.throwError("Cannot use string as placement offset.", "Try using a integral value instead.", this); },
                    [](auto &&offset) -> u64 { return offset; } },
                offset->getValue());

                if (evaluator->dataOffset() < evaluator->getDataBaseAddress() || evaluator->dataOffset() > evaluator->getDataBaseAddress() + evaluator->getDataSize())
                    err::E0005.throwError(fmt::format("Cannot place variable '{}' at out of bounds address 0x{:08X}", this->m_name, evaluator->dataOffset()), { }, this);
            }

            auto patterns = this->m_type->createPatterns(evaluator);
            if (patterns.empty())
                err::E0005.throwError("'auto' can only be used with parameters.", { }, this);

            auto &pattern = patterns.front();
            if (this->m_placementOffset != nullptr && dynamic_cast<ptrn::PatternString*>(pattern.get()) != nullptr)
                err::E0005.throwError(fmt::format("Variables of type 'str' cannot be placed in memory.", this->m_name), { }, this);

            pattern->setVariableName(this->m_name);

            if (this->m_placementSection != nullptr)
                pattern->setSection(evaluator->getSectionId());

            applyVariableAttributes(evaluator, this, pattern);


            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            }

            if (evaluator->getSectionId() == ptrn::Pattern::PatternLocalSectionId) {
                evaluator->dataOffset() = startOffset;
                this->execute(evaluator);
                return { };
            } else {
                return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>(std::move(pattern));
            }
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            evaluator->createVariable(this->getName(), this->getType().get(), { }, this->m_outVariable, false, false, this->m_constant);

            if (this->m_placementOffset != nullptr) {
                const auto placementNode = this->m_placementOffset->evaluate(evaluator);
                const auto offsetLiteral = dynamic_cast<ASTNodeLiteral *>(placementNode.get());
                if (offsetLiteral == nullptr)
                    err::E0002.throwError("Void expression used in placement expression.", { }, this);


                u64 section = 0;
                if (this->m_placementSection != nullptr) {
                    const auto sectionNode = this->m_placementSection->evaluate(evaluator);
                    const auto sectionLiteral = dynamic_cast<ASTNodeLiteral *>(sectionNode.get());
                    if (sectionLiteral == nullptr)
                        err::E0002.throwError("Cannot use void expression as section identifier.", {}, this);

                    section = sectionLiteral->getValue().toUnsigned();
                }

                evaluator->setVariableAddress(this->getName(), offsetLiteral->getValue().toUnsigned(), section);
            }

            return std::nullopt;
        }

        [[nodiscard]] bool isConstant() const {
            return this->m_constant;
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::unique_ptr<ASTNode> m_placementOffset, m_placementSection;

        bool m_inVariable = false, m_outVariable = false;
        bool m_constant = false;
    };

}