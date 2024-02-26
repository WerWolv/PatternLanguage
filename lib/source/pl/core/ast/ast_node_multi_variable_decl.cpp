#include <pl/core/ast/ast_node_multi_variable_decl.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_variable_decl.hpp>

namespace pl::core::ast {

    ASTNodeMultiVariableDecl::ASTNodeMultiVariableDecl(std::vector<std::shared_ptr<ASTNode>> &&variables) : m_variables(std::move(variables)) { }

    ASTNodeMultiVariableDecl::ASTNodeMultiVariableDecl(const ASTNodeMultiVariableDecl &other) : ASTNode(other) {
        for (auto &variable : other.m_variables)
            this->m_variables.push_back(variable->clone());
    }


    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeMultiVariableDecl::createPatterns(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::vector<std::shared_ptr<ptrn::Pattern>> patterns;

        for (auto &node : this->m_variables) {
            auto newPatterns = node->createPatterns(evaluator);
            std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(patterns));
        }

        return patterns;
    }

    ASTNode::FunctionResult ASTNodeMultiVariableDecl::execute(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        for (auto &variable : this->m_variables) {
            auto variableDecl = dynamic_cast<ASTNodeVariableDecl *>(variable.get());
            auto variableType = variableDecl->getType();

            evaluator->createVariable(variableDecl->getName(), variableType.get());
        }

        return std::nullopt;
    }

}