#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeFunctionDefinition : public ASTNode {
    public:
        ASTNodeFunctionDefinition(std::string name, std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> &&params, std::vector<std::unique_ptr<ASTNode>> &&body, std::optional<std::string> parameterPack, std::vector<std::unique_ptr<ASTNode>> &&defaultParameters);
        ASTNodeFunctionDefinition(const ASTNodeFunctionDefinition &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeFunctionDefinition(*this));
        }

        [[nodiscard]] const std::string &getName() const {
            return this->m_name;
        }

        [[nodiscard]] const auto &getParams() const {
            return this->m_params;
        }

        [[nodiscard]] const auto &getBody() const {
            return this->m_body;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;

        [[nodiscard]] const auto &getParameterPack() const {
            return this->m_parameterPack;
        }

        [[nodiscard]] const auto &getDefaultParameters() const {
            return this->m_defaultParameters;
        }


    private:
        std::string m_name;
        std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> m_params;
        std::vector<std::unique_ptr<ASTNode>> m_body;
        std::optional<std::string> m_parameterPack;
        std::vector<std::unique_ptr<ASTNode>> m_defaultParameters;
    };

}