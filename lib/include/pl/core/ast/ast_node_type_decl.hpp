#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>
#include <pl/core/ast/ast_node_template_parameter.hpp>

#include <wolv/utils/guards.hpp>

#include <bit>

namespace pl::core::ast {

    class ASTNodeTypeApplication;

    class ASTNodeTypeDecl : public ASTNode,
                            public Attributable {
    public:
        explicit ASTNodeTypeDecl(std::string name);
        ASTNodeTypeDecl(std::string name, std::shared_ptr<ASTNode> type);
        ASTNodeTypeDecl(const ASTNodeTypeDecl &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeDecl(*this));
        }

        void setName(const std::string &name) {
            this->m_name = name;
        }
        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] const std::shared_ptr<ASTNode> &getType() const;

        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;
        void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) override;

        [[nodiscard]] bool isValid() const {
            return this->m_type != nullptr;
        }

        [[nodiscard]] bool isTemplateType() const {
            return !this->m_templateParameters.empty();
        }

        [[nodiscard]] bool isForwardDeclared() const {
            return this->m_forwardDeclared;
        }

        void setCompleted(bool completed = true) {
            this->m_completed = completed;
        }

        void setType(std::shared_ptr<ASTNode> type) {
            this->m_type = std::move(type);
        }


        [[nodiscard]] const std::vector<std::shared_ptr<ASTNodeTemplateParameter>> &getTemplateParameters() const {
            return this->m_templateParameters;
        }

        void setTemplateParameters(std::vector<std::shared_ptr<ASTNodeTemplateParameter>> &&types) {
            this->m_templateParameters = std::move(types);
        }

        [[nodiscard]] const std::string getTypeName() const;

        const ASTNode* getTypeDefinition(Evaluator *evaluator) const;

    private:
        bool m_forwardDeclared = false;
        bool m_completed = false;
        mutable bool m_alreadyCopied = false;

        std::string m_name;
        std::shared_ptr<ASTNode> m_type;
        std::vector<std::shared_ptr<ASTNodeTemplateParameter>> m_templateParameters;
        std::vector<std::unique_ptr<ASTNode>> m_templateArguments;

        mutable std::unique_ptr<ASTNodeTypeApplication> m_currTemplateParameterType;
    };

}