#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeScopeResolution : public ASTNode {
    public:
        explicit ASTNodeScopeResolution(std::unique_ptr<ASTNode> &&type, std::string name) : ASTNode(), m_type(std::move(type)), m_name(std::move(name)) { }

        ASTNodeScopeResolution(const ASTNodeScopeResolution &other) : ASTNode(other) {
            this->m_type = other.m_type->clone();
            this->m_name = other.m_name;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeScopeResolution(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            auto type = this->m_type->evaluate(evaluator);

            if (auto enumType = dynamic_cast<ASTNodeEnum *>(type.get())) {
                for (auto &[name, values] : enumType->getEntries()) {
                    if (name == this->m_name)
                        return values.first->evaluate(evaluator);
                }
            } else {
                err::E0004.throwError("Invalid scope resolution. This cannot be accessed using the scope resolution operator.", {}, this);
            }

            err::E0004.throwError(fmt::format("Cannot find constant '{}' in this type.", this->m_name), {}, this);
        }

    private:
        std::unique_ptr<ASTNode> m_type;
        std::string m_name;
    };

}