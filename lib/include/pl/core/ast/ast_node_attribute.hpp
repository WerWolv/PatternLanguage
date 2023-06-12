#pragma once

#include <pl/core/ast/ast_node.hpp>

namespace pl::core::ast {

    class ASTNodeAttribute : public ASTNode {
    public:
        explicit ASTNodeAttribute(std::string attribute, std::vector<std::unique_ptr<ASTNode>> &&value = {});
        ASTNodeAttribute(const ASTNodeAttribute &other);
        ~ASTNodeAttribute() override = default;

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeAttribute(*this));
        }

        [[nodiscard]] const std::string &getAttribute() const {
            return this->m_attribute;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getArguments() const {
            return this->m_value;
        }

    private:
        std::string m_attribute;
        std::vector<std::unique_ptr<ASTNode>> m_value;
    };


    class Attributable {
    protected:
        Attributable() = default;
        Attributable(const Attributable &other);

    public:
        virtual void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute);
        [[nodiscard]] const std::vector<std::unique_ptr<ASTNodeAttribute>> &getAttributes() const;
        [[nodiscard]] ASTNodeAttribute *getAttributeByName(const std::string &key) const;
        [[nodiscard]] bool hasAttribute(const std::string &key, bool needsParameter) const;

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>>& getAttributeArguments(const std::string &key) const;
        [[nodiscard]] std::shared_ptr<ASTNode> getFirstAttributeValue(const std::vector<std::string> &keys) const;

    private:
        std::vector<std::unique_ptr<ASTNodeAttribute>> m_attributes;
    };

    void applyTypeAttributes(Evaluator *evaluator, const ASTNode *node, const std::shared_ptr<ptrn::Pattern> &pattern);
    void applyVariableAttributes(Evaluator *evaluator, const ASTNode *node, const std::shared_ptr<ptrn::Pattern> &pattern);

}
