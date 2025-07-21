#include <pl/core/ast/ast_node_bitfield_field.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

#include <pl/patterns/pattern_boolean.hpp>

namespace pl::core::ast {

    ASTNodeBitfieldField::ASTNodeBitfieldField(std::string name, std::unique_ptr<ASTNode> &&size)
    : ASTNode(), m_name(std::move(name)), m_size(std::move(size)) { }

    ASTNodeBitfieldField::ASTNodeBitfieldField(const ASTNodeBitfieldField &other) : ASTNode(other), Attributable(other) {
        this->m_name = other.m_name;
        this->m_size = other.m_size->clone();
    }


    [[nodiscard]] const std::string &ASTNodeBitfieldField::getName() const { return this->m_name; }
    [[nodiscard]] const std::unique_ptr<ASTNode> &ASTNodeBitfieldField::getSize() const { return this->m_size; }

    [[nodiscard]] bool ASTNodeBitfieldField::isPadding() const { return this->getName() == "$padding$"; }

    [[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> ASTNodeBitfieldField::createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const {
        return std::make_shared<ptrn::PatternBitfieldField>(evaluator, byteOffset, bitOffset, bitSize, getLocation().line);
    }

    void ASTNodeBitfieldField::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto node = this->m_size->evaluate(evaluator);
        auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());
        if (literal == nullptr)
            err::E0010.throwError("Cannot use void expression as bitfield field size.", {}, this->getLocation());

        u8 bitSize = std::visit(wolv::util::overloaded {
                [this](const std::string &) -> u8 { err::E0005.throwError("Cannot use string as bitfield field size.", "Try using a integral value instead.", this->m_size->getLocation()); },
                [this](const std::shared_ptr<ptrn::Pattern>&) -> u8 { err::E0005.throwError("Cannot use string as bitfield field size.", "Try using a integral value instead.", this->m_size->getLocation()); },
                [](auto &&offset) -> u8 { return static_cast<u8>(offset); }
        }, literal->getValue());

        auto position = evaluator->getBitwiseReadOffsetAndIncrement(bitSize);
        auto pattern = this->createBitfield(evaluator, position.byteOffset, position.bitOffset, bitSize);
        pattern->setPadding(this->isPadding());
        pattern->setVariableName(this->getName(), this->getLocation());

        pattern->setEndian(evaluator->getDefaultEndian());
        pattern->setSection(evaluator->getSectionId());

        applyVariableAttributes(evaluator, this, pattern);

        resultPatterns = hlp::moveToVector<std::shared_ptr<ptrn::Pattern>>({ std::move(pattern) });
    }


    [[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> ASTNodeBitfieldFieldSigned::createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const {
        return std::make_shared<ptrn::PatternBitfieldFieldSigned>(evaluator, byteOffset, bitOffset, bitSize, getLocation().line);
    }


    ASTNodeBitfieldFieldSizedType::ASTNodeBitfieldFieldSizedType(std::string name, std::unique_ptr<ASTNodeTypeDecl> &&type, std::unique_ptr<ASTNode> &&size)
    : ASTNodeBitfieldField(std::move(name), std::move(size)), m_type(std::move(type)) { }

    ASTNodeBitfieldFieldSizedType::ASTNodeBitfieldFieldSizedType(const ASTNodeBitfieldFieldSizedType &other) : ASTNodeBitfieldField(other) {
        this->m_type = std::unique_ptr<ASTNodeTypeDecl>(static_cast<ASTNodeTypeDecl*>(other.m_type->clone().release()));
    }

[[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> ASTNodeBitfieldFieldSizedType::createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const {
    auto originalPosition = evaluator->getBitwiseReadOffset();
    evaluator->setBitwiseReadOffset(byteOffset, bitOffset);

    std::vector<std::shared_ptr<ptrn::Pattern>> patterns;
    this->m_type->createPatterns(evaluator, patterns);
    auto &pattern = patterns[0];
    std::shared_ptr<ptrn::PatternBitfieldField> result = nullptr;
    evaluator->setBitwiseReadOffset(originalPosition);

    if (auto *patternEnum = dynamic_cast<ptrn::PatternEnum *>(pattern.get()); patternEnum != nullptr) {
        auto bitfieldEnum = std::make_unique<ptrn::PatternBitfieldFieldEnum>(evaluator, byteOffset, bitOffset, bitSize, getLocation().line);
        bitfieldEnum->setTypeName(patternEnum->getTypeName());
        bitfieldEnum->setEnumValues(patternEnum->getEnumValues());
        result = std::move(bitfieldEnum);
    } else if (dynamic_cast<ptrn::PatternBoolean *>(pattern.get()) != nullptr) {
        result = std::make_shared<ptrn::PatternBitfieldFieldBoolean>(evaluator, byteOffset, bitOffset, bitSize, getLocation().line);
    } else {
        err::E0004.throwError("Bit size specifiers may only be used with unsigned, signed, bool or enum types.", {}, this->getLocation());
    }

    return result;
}


}