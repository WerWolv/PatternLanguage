#include <pl/core/ast/ast_node_scope_resolution.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeScopeResolution::ASTNodeScopeResolution(std::shared_ptr<ASTNode> &&type, std::string name)
        : m_type(std::move(type)), m_name(std::move(name)) { }

    ASTNodeScopeResolution::ASTNodeScopeResolution(const ASTNodeScopeResolution &other) : ASTNode(other) {
        this->m_type = other.m_type;
        this->m_name = other.m_name;
    }


    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeScopeResolution::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        auto type = this->m_type->evaluate(evaluator);

        if (auto enumType = dynamic_cast<ASTNodeEnum *>(type.get())) {
            for (auto &[min, max, name] : enumType->getEnumValues(evaluator)) {
                if (name == this->m_name)
                    return std::make_unique<ASTNodeLiteral>(min);
            }
        } else {
            err::E0004.throwError("Invalid scope resolution. This cannot be accessed using the scope resolution operator.", {}, this->getLocation());
        }

        err::E0004.throwError(fmt::format("Cannot find constant '{}' in this type.", this->m_name), {}, this->getLocation());
    }

}
