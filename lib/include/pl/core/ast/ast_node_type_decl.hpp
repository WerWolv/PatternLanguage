#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <wolv/utils/guards.hpp>

#include <bit>

namespace pl::core::ast {

    class ASTNodeTypeDecl : public ASTNode,
                            public Attributable {
    public:
        explicit ASTNodeTypeDecl(std::string name);
        ASTNodeTypeDecl(std::string name, std::shared_ptr<ASTNode> type, std::optional<std::endian> endian = std::nullopt);
        ASTNodeTypeDecl(const ASTNodeTypeDecl &other);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeDecl(*this));
        }

        void setName(const std::string &name) {
            this->m_name = name;
        }
        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] const std::shared_ptr<ASTNode> &getType() const;
        [[nodiscard]] std::optional<std::endian> getEndian() const { return this->m_endian; }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;
        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override;
        void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) override;

        [[nodiscard]] bool isValid() const {
            return this->m_valid;
        }

        [[nodiscard]] bool isTemplateType() const {
            return this->m_templateType;
        }

        [[nodiscard]] bool isForwardDeclared() const {
            return this->m_forwardDeclared;
        }

        void setReference(bool reference) {
            this->m_reference = reference;
        }

        [[nodiscard]] bool isReference() const {
            return this->m_reference;
        }

        void setCompleted() {
            this->m_completed = true;
        }

        void setType(std::shared_ptr<ASTNode> type, bool templateType = false) {
            this->m_valid = true;
            this->m_templateType = templateType;
            this->m_type = std::move(type);
        }

        void setEndian(std::endian endian) {
            this->m_endian = endian;
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getTemplateParameters() const {
            return this->m_templateParameters;
        }

        void setTemplateParameters(std::vector<std::shared_ptr<ASTNode>> &&types) {
            if (!types.empty())
                this->m_templateType = true;

            this->m_templateParameters = std::move(types);
        }

    private:
        bool m_forwardDeclared = false;
        bool m_valid = true;
        bool m_templateType = false;
        bool m_completed = false;

        std::string m_name;
        std::shared_ptr<ASTNode> m_type;
        std::optional<std::endian> m_endian;
        std::vector<std::shared_ptr<ASTNode>> m_templateParameters;
        bool m_reference = false;

        mutable std::unique_ptr<ASTNodeTypeDecl> m_currTemplateParameterType;
    };

}