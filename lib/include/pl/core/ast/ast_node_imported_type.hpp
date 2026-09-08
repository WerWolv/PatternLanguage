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

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

        [[nodiscard]] const std::string &getImportedTypename() const {
            return m_importedTypeName;
        }

        void accept(vis::ASTVisitor &v) override {
            v.visit(*this);
        }

    private:
        std::string m_importedTypeName;
    };

}