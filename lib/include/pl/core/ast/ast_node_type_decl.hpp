#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_attribute.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <wolv/utils/guards.hpp>

namespace pl::core::ast {

    class ASTNodeTypeDecl : public ASTNode,
                            public Attributable {
    public:
        explicit ASTNodeTypeDecl(std::string name) : m_forwardDeclared(true), m_valid(false), m_name(std::move(name)) { }

        ASTNodeTypeDecl(std::string name, std::shared_ptr<ASTNode> type, std::optional<std::endian> endian = std::nullopt)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_endian(endian) { }

        ASTNodeTypeDecl(const ASTNodeTypeDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name                = other.m_name;

            if (other.m_type != nullptr) {
                if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(other.m_type.get()); (typeDecl != nullptr && typeDecl->isForwardDeclared() && !typeDecl->isTemplateType()) || other.m_completed)
                    this->m_type = other.m_type;
                else
                    this->m_type = other.m_type->clone();
            }

            this->m_endian              = other.m_endian;
            this->m_forwardDeclared     = other.m_forwardDeclared;
            this->m_reference           = other.m_reference;
            this->m_completed           = other.m_completed;
            this->m_valid               = other.m_valid;
            this->m_templateType        = other.m_templateType;

            for (const auto &templateParameter : other.m_templateParameters) {
                this->m_templateParameters.push_back(templateParameter->clone());
            }
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeDecl(*this));
        }

        void setName(const std::string &name) {
            this->m_name = name;
        }
        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] const std::shared_ptr<ASTNode> &getType() const {
            if (!this->isValid())
                err::E0004.throwError(fmt::format("Cannot use incomplete type '{}' before it has been defined.", this->m_name), "Try defining this type further up in your code before trying to instantiate it.", this);

            return this->m_type;
        }
        [[nodiscard]] std::optional<std::endian> getEndian() const { return this->m_endian; }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            auto type = this->getType()->evaluate(evaluator);

            if (auto attributable = dynamic_cast<Attributable *>(type.get())) {
                for (auto &attribute : this->getAttributes()) {
                    auto copy = attribute->clone();
                    if (auto node = dynamic_cast<ASTNodeAttribute *>(copy.get())) {
                        attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(node));
                        (void)copy.release();
                    }
                }
            }

            return type;
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            std::vector<std::unique_ptr<ASTNodeLiteral>> templateParamLiterals(this->m_templateParameters.size());

            // Get template parameter values before pushing a new template parameter scope so that variables from the parent scope are used before being overwritten.
            for (size_t i = 0; i < this->m_templateParameters.size(); i++) {
                auto &templateParameter = this->m_templateParameters[i];
                if (auto lvalue = dynamic_cast<ASTNodeLValueAssignment *>(templateParameter.get())) {
                    if (!lvalue->getRValue())
                        err::E0003.throwError(fmt::format("No value set for non-type template parameter {}. This is a bug.", lvalue->getLValueName()), {}, this);
                    auto value = lvalue->getRValue()->evaluate(evaluator);
                    if (auto literal = dynamic_cast<ASTNodeLiteral*>(value.release()))
                        templateParamLiterals[i] = std::unique_ptr<ASTNodeLiteral>(literal);
                    else
                        err::E0003.throwError(fmt::format("Template parameter {} is not a literal. This is a bug.", lvalue->getLValueName()), {}, this);
                }
            }

            evaluator->pushTemplateParameters();
            ON_SCOPE_EXIT {
                evaluator->popTemplateParameters();
            };

            {
                evaluator->pushSectionId(ptrn::Pattern::HeapSectionId);
                ON_SCOPE_EXIT {
                    evaluator->popSectionId();
                };

                for (size_t i = 0; i < this->m_templateParameters.size(); i++) {
                    auto &templateParameter = this->m_templateParameters[i];
                    if (auto lvalue = dynamic_cast<ASTNodeLValueAssignment *>(templateParameter.get())) {
                        auto value = templateParamLiterals[i]->getValue();

                        // Allow the evaluator to throw an error at the correct source location.
                        m_currentTemplateParameterType->setSourceLocation(lvalue->getLine(), lvalue->getColumn());

                        auto variable = evaluator->createVariable(lvalue->getLValueName(), m_currentTemplateParameterType.get(), value, false, false, true);
                        if (variable != nullptr)
                            evaluator->setVariable(variable.get(), value);
                    }
                }
            }

            auto currEndian = evaluator->getDefaultEndian();
            ON_SCOPE_EXIT { evaluator->setDefaultEndian(currEndian); };

            evaluator->setDefaultEndian(this->m_endian.value_or(currEndian));
            auto patterns = this->getType()->createPatterns(evaluator);

            for (auto &pattern : patterns) {
                if (pattern == nullptr)
                    continue;

                if (!pattern->hasOverriddenEndian())
                    pattern->setEndian(evaluator->getDefaultEndian());

                if (!this->m_name.empty())
                    pattern->setTypeName(this->m_name);

                if (auto iteratable = dynamic_cast<ptrn::Iteratable *>(pattern.get()); iteratable != nullptr) {
                    auto scope = iteratable->getEntries();
                    evaluator->pushScope(pattern, scope);
                    applyTypeAttributes(evaluator, this, pattern);
                    evaluator->popScope();
                } else {
                    applyTypeAttributes(evaluator, this, pattern);
                }

            }

            return patterns;
        }

        void addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) override {
            if (this->isValid()) {
                if (auto attributable = dynamic_cast<Attributable *>(this->getType().get()); attributable != nullptr) {
                    attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(static_cast<ASTNodeAttribute *>(attribute->clone().release())));
                }
            }

            Attributable::addAttribute(std::move(attribute));
        }

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

        mutable std::unique_ptr<ASTNodeBuiltinType> m_currentTemplateParameterType = std::make_unique<ASTNodeBuiltinType>(Token::ValueType::Auto);
    };

}