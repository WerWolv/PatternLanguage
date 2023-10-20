#include <pl/core/ast/ast_node_type_decl.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>

namespace pl::core::ast {

    ASTNodeTypeDecl::ASTNodeTypeDecl(std::string name) : m_forwardDeclared(true), m_valid(false), m_name(std::move(name)) { }

    ASTNodeTypeDecl::ASTNodeTypeDecl(std::string name, std::shared_ptr<ASTNode> type, std::optional<std::endian> endian)
    : m_name(std::move(name)), m_type(std::move(type)), m_endian(endian) { }

    ASTNodeTypeDecl::ASTNodeTypeDecl(const ASTNodeTypeDecl &other) : ASTNode(other), Attributable(other) {
        this->m_name                = other.m_name;

        if (other.m_type != nullptr) {
            if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(other.m_type.get()); (typeDecl != nullptr && typeDecl->isForwardDeclared() && !typeDecl->isTemplateType()) || other.m_completed || other.m_alreadyCopied)
                this->m_type = other.m_type;
            else {
                other.m_alreadyCopied = true;
                this->m_type = other.m_type->clone();
                other.m_alreadyCopied = false;
            }
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

    [[nodiscard]] const std::shared_ptr<ASTNode>& ASTNodeTypeDecl::getType() const {
        if (!this->isValid())
            err::E0004.throwError(fmt::format("Cannot use incomplete type '{}' before it has been defined.", this->m_name), "Try defining this type further up in your code before trying to instantiate it.", this);

        return this->m_type;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeTypeDecl::evaluate(Evaluator *evaluator) const {
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

    [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeTypeDecl::createPatterns(Evaluator *evaluator) const {
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

        std::vector<std::shared_ptr<ptrn::Pattern>> templatePatterns;

        {
            evaluator->pushSectionId(ptrn::Pattern::PatternLocalSectionId);
            ON_SCOPE_EXIT {
                evaluator->popSectionId();
            };

            for (size_t i = 0; i < this->m_templateParameters.size(); i++) {
                auto &templateParameter = this->m_templateParameters[i];
                if (auto lvalue = dynamic_cast<ASTNodeLValueAssignment *>(templateParameter.get())) {
                    auto value = templateParamLiterals[i]->getValue();

                    // Allow the evaluator to throw an error at the correct source location.
                    if (this->m_currTemplateParameterType == nullptr) {
                        this->m_currTemplateParameterType = std::make_unique<ASTNodeTypeDecl>("");
                        this->m_currTemplateParameterType->setType(std::make_unique<ASTNodeBuiltinType>(Token::ValueType::Auto));
                    }

                    this->m_currTemplateParameterType->setSourceLocation(lvalue->getLine(), lvalue->getColumn());

                    auto variable = evaluator->createVariable(lvalue->getLValueName(), this->m_currTemplateParameterType.get(), value, false, value.isPattern(), true, true);
                    if (variable != nullptr) {
                        variable->setInitialized(false);
                        evaluator->setVariable(variable, value);
                        templatePatterns.push_back(variable);

                        variable->setVisibility(ptrn::Visibility::Hidden);
                    }
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

            if (auto iterable = dynamic_cast<ptrn::IIterable *>(pattern.get()); iterable != nullptr) {
                auto scope = iterable->getEntries();
                std::move(templatePatterns.begin(), templatePatterns.end(), std::back_inserter(scope));

                evaluator->pushScope(pattern, scope);
                ON_SCOPE_EXIT { evaluator->popScope(); };
                applyTypeAttributes(evaluator, this, pattern);

                iterable->setEntries(std::move(scope));
                templatePatterns.clear();
            } else {
                applyTypeAttributes(evaluator, this, pattern);
            }

        }

        return patterns;
    }

    void ASTNodeTypeDecl::addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) {
        if (this->isValid()) {
            if (auto attributable = dynamic_cast<Attributable *>(this->getType().get()); attributable != nullptr) {
                attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(static_cast<ASTNodeAttribute *>(attribute->clone().release())));
            }
        }

        Attributable::addAttribute(std::move(attribute));
    }

}