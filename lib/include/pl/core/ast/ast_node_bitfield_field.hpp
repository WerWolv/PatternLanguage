#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::core::ast {

    class ASTNodeTypeDecl;

    class ASTNodeBitfieldField : public ASTNode,
                                 public Attributable {
    public:
        ASTNodeBitfieldField(std::string name, std::unique_ptr<ASTNode> &&size);
        ASTNodeBitfieldField(const ASTNodeBitfieldField &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfieldField(*this));
        }

        [[nodiscard]] const std::string &getName() const;
        [[nodiscard]] const std::unique_ptr<ASTNode> &getSize() const;

        [[nodiscard]] bool isPadding() const;

        [[nodiscard]] virtual std::shared_ptr<ptrn::PatternBitfieldField> createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const;
        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;

    private:
        std::string m_name;
        std::unique_ptr<ASTNode> m_size;
    };

    class ASTNodeBitfieldFieldSigned : public ASTNodeBitfieldField {
    public:
        using ASTNodeBitfieldField::ASTNodeBitfieldField;

        [[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const override;
    };

    class ASTNodeBitfieldFieldSizedType : public ASTNodeBitfieldField {
    public:
        ASTNodeBitfieldFieldSizedType(std::string name, std::unique_ptr<ASTNodeTypeDecl> &&type, std::unique_ptr<ASTNode> &&size);
        ASTNodeBitfieldFieldSizedType(const ASTNodeBitfieldFieldSizedType &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfieldFieldSizedType(*this));
        }

        [[nodiscard]] std::shared_ptr<ptrn::PatternBitfieldField> createBitfield(Evaluator *evaluator, u64 byteOffset, u8 bitOffset, u8 bitSize) const override;

    private:
        std::unique_ptr<ASTNodeTypeDecl> m_type;
    };

}