#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::core::ast {

    class ASTNodeBitfieldField : public ASTNode,
                                 public Attributable {
    public:
        ASTNodeBitfieldField(std::string name, std::unique_ptr<ASTNode> &&size)
            : ASTNode(), m_name(std::move(name)), m_size(std::move(size)) { }

        ASTNodeBitfieldField(const ASTNodeBitfieldField &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_size = other.m_size->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfieldField(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getSize() const { return this->m_size; }

        [[nodiscard]] bool isPadding() const { return this->getName() == "$padding$"; }

        [[nodiscard]] virtual std::shared_ptr<ptrn::PatternBitfieldField> createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const {
            return std::make_shared<ptrn::PatternBitfieldField>(evaluator, byteOffset, bitOffset, bitSize);
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto node = this->m_size->evaluate(evaluator);
            auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
            if (literal == nullptr)
                err::E0010.throwError("Cannot use void expression as bitfield field size.", {}, this);

            u8 bitSize = std::visit(wolv::util::overloaded {
                    [this](const std::string &) -> u8 { err::E0005.throwError("Cannot use string as bitfield field size.", "Try using a integral value instead.", this); },
                    [this](ptrn::Pattern *) -> u8 { err::E0005.throwError("Cannot use string as bitfield field size.", "Try using a integral value instead.", this); },
                    [](auto &&offset) -> u8 { return static_cast<u8>(offset); }
            }, literal->getValue());

            std::shared_ptr<ptrn::PatternBitfieldField> pattern;
            if (evaluator->isBitfieldReversed()) {
                evaluator->incrementBitfieldBitOffset(-bitSize);
                pattern = this->createBitfield(evaluator, evaluator->dataOffset(), evaluator->getBitfieldBitOffset(), bitSize);
            } else {
                pattern = this->createBitfield(evaluator, evaluator->dataOffset(), evaluator->getBitfieldBitOffset(), bitSize);
                evaluator->incrementBitfieldBitOffset(bitSize);
            }
            pattern->setPadding(this->isPadding());
            pattern->setVariableName(this->getName());

            pattern->setEndian(evaluator->getDefaultEndian());
            pattern->setSection(evaluator->getSectionId());

            applyVariableAttributes(evaluator, this, pattern);

            return hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>({ std::move(pattern) });
        }

    private:
        std::string m_name;
        std::unique_ptr<ASTNode> m_size;
    };

    class ASTNodeBitfieldFieldSigned : public ASTNodeBitfieldField {
    public:
        using ASTNodeBitfieldField::ASTNodeBitfieldField;

        [[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const override {
            return std::make_shared<ptrn::PatternBitfieldFieldSigned>(evaluator, byteOffset, bitOffset, bitSize);
        }
    };

}