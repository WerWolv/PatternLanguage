#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeImportedType : public ASTNode {
    public:
        explicit ASTNodeImportedType(const std::string &importedTypeName);
        ASTNodeImportedType(const ASTNodeImportedType &other) = default;

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeImportedType(*this));
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;
        std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

    private:
        std::string m_importedTypeName;
    };

}