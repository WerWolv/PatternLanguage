#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

namespace pl::core::ast {

    class ASTNodeTypeApplication : public ASTNode {
    public:
        explicit ASTNodeTypeApplication(std::shared_ptr<ASTNode> type);

        ASTNodeTypeApplication(const ASTNodeTypeApplication &);

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeApplication(*this));
        }

        void setTemplateArguments(std::vector<std::unique_ptr<ASTNode>> &&arguments) {
            this->m_templateArguments = std::move(arguments);
        }
        
        std::vector<std::unique_ptr<ASTNode>> evaluateTemplateArguments(Evaluator *evaluator) const;

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override;
        void createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const override;

        [[nodiscard]] const std::shared_ptr<ASTNode> &getType() const {
            return this->m_type;
        }
    
        void setReference(bool reference) {
            this->m_reference = reference;
        }

        [[nodiscard]] bool isReference() const {
            return this->m_reference;
        }

        const ast::ASTNode* getTypeDefinition(Evaluator *evaluator) const;

        [[nodiscard]] const std::string getTypeName() const;

        void setEndian(std::endian endian) {
            this->m_endian = endian;
        }

        [[nodiscard]] std::optional<std::endian> getEndian() const { return this->m_endian; }

        void setTemplateParameterIndex(int index) {
            this->m_templateParameterIndex = index;
        }

        [[nodiscard]] int getTemplateParameterIndex() const {
            return this->m_templateParameterIndex;
        }

    private:
        std::shared_ptr<ASTNode> m_type;
        std::vector<std::unique_ptr<ASTNode>> m_templateArguments;
        bool m_reference = false;
        std::optional<std::endian> m_endian;
        size_t m_templateParameterIndex = 0;
    };

}