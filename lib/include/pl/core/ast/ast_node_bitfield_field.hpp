#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

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
                    [this](const std::string &) -> u8 { err::E0005.throwError("Cannot use string as bitfield field size.", "Try using a integral value instead.", this->m_size.get()); },
                    [this](ptrn::Pattern *) -> u8 { err::E0005.throwError("Cannot use string as bitfield field size.", "Try using a integral value instead.", this->m_size.get()); },
                    [](auto &&offset) -> u8 { return static_cast<u8>(offset); }
            }, literal->getValue());

            std::shared_ptr<ptrn::PatternBitfieldField> pattern;
            auto position = evaluator->getBitwiseReadOffsetAndIncrement(bitSize);
            pattern = this->createBitfield(evaluator, position.byteOffset, position.bitOffset, bitSize);
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

    class ASTNodeBitfieldFieldSizedType : public ASTNodeBitfieldField {
    public:
        ASTNodeBitfieldFieldSizedType(std::string name, std::unique_ptr<ASTNodeTypeDecl> &&type, std::unique_ptr<ASTNode> &&size)
            : ASTNodeBitfieldField(std::move(name), std::move(size)), m_type(std::move(type)) { }

        ASTNodeBitfieldFieldSizedType(const ASTNodeBitfieldFieldSizedType &other) : ASTNodeBitfieldField(other) {
            this->m_type = std::unique_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfieldFieldSizedType(*this));
        }

        [[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const override {
            auto originalPosition = evaluator->getBitwiseReadOffset();
            evaluator->setBitwiseReadOffset(byteOffset, bitOffset);
            auto patterns = m_type->createPatterns(evaluator);
            auto &pattern = patterns[0];
            std::shared_ptr<ptrn::PatternBitfieldField> result = nullptr;
            evaluator->setBitwiseReadOffset(originalPosition);

            if (auto *patternEnum = dynamic_cast<ptrn::PatternEnum*>(pattern.get()); patternEnum != nullptr) {
                auto bitfieldEnum = std::make_unique<ptrn::PatternBitfieldFieldEnum>(evaluator, byteOffset, bitOffset, bitSize);
                bitfieldEnum->setTypeName(patternEnum->getTypeName());
                bitfieldEnum->setEnumValues(patternEnum->getEnumValues());
                result = std::move(bitfieldEnum);
            } else {
                err::E0004.throwError("Can only use enums as sized bitfield fields.", {}, this);
            }
            return result;
        }

    private:
        std::unique_ptr<ASTNodeTypeDecl> m_type;
    };

}