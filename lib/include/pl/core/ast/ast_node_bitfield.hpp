#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_bitfield_field.hpp>

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::core::ast {

    class ASTNodeBitfield : public ASTNode,
                            public Attributable {
    public:
        enum class BitfieldOrder {
            MostToLeastSignificant = 0,
            LeastToMostSignificant = 1,
        };

        ASTNodeBitfield() = default;
        ASTNodeBitfield(const ASTNodeBitfield &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfield(*this));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getEntries() const;
        void addEntry(std::unique_ptr<ASTNode> &&entry);

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;

    private:
        std::vector<std::shared_ptr<ASTNode>> m_entries;
    };

}