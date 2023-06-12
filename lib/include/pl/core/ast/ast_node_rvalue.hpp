#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeRValue : public ASTNode {
    public:
        using PathSegment = std::variant<std::string, std::unique_ptr<ASTNode>>;
        using Path        = std::vector<PathSegment>;

        explicit ASTNodeRValue(Path &&path);
        ASTNodeRValue(const ASTNodeRValue &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeRValue(*this));
        }

        [[nodiscard]] const Path &getPath() const {
            return this->m_path;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;
        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;

    private:
        Path m_path;
    };

}