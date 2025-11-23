#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_template_parameter.hpp>
#include <pl/core/ast/ast_node_type_appilication.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>

namespace pl::core::ast {

    ASTNodeTypeDecl::ASTNodeTypeDecl(std::string name) : m_forwardDeclared(true), m_name(std::move(name)) { }

    ASTNodeTypeDecl::ASTNodeTypeDecl(std::string name, std::shared_ptr<ASTNode> type)
    : m_name(std::move(name)), m_type(std::move(type)){ }

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

        this->m_forwardDeclared     = other.m_forwardDeclared;
        this->m_completed           = other.m_completed;

        for (const auto &templateParameter : other.m_templateParameters) {
            this->m_templateParameters.push_back(std::shared_ptr<ASTNodeTemplateParameter>(dynamic_cast<ASTNodeTemplateParameter*>(templateParameter->clone().release())));
        }
        for (const auto &templateArguments : other.m_templateArguments) {
            this->m_templateArguments.push_back(templateArguments->clone());
        }
    }

    [[nodiscard]] const std::shared_ptr<ASTNode>& ASTNodeTypeDecl::getType() const {
        if (!this->isValid())
            err::E0004.throwError(fmt::format("Cannot use incomplete type '{}' before it has been defined.", this->m_name), "Try defining this type further up in your code before trying to instantiate it.", this->getLocation());

        return this->m_type;
    }

    void ASTNodeTypeDecl::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::vector<std::shared_ptr<ptrn::Pattern>> dummyPatterns;

        bool requiresNewScope = dynamic_cast<ASTNodeTypeApplication*>(this->getType().get()) != nullptr;

        evaluator->pushTemplateParameters();
        evaluator->pushTypeTemplateParameters();

        if(requiresNewScope) {
            evaluator->pushScope(nullptr, dummyPatterns);
        }

        ON_SCOPE_EXIT {
                evaluator->popTemplateParameters();
                evaluator->popTypeTemplateParameters();
                if (requiresNewScope) {
                    evaluator->popScope();
                }
        };

        auto& templateArguments = evaluator->getCurrentTemplateArguments();

        std::vector<std::shared_ptr<ptrn::Pattern>> templatePatterns;

        {
            evaluator->pushSectionId(ptrn::Pattern::PatternLocalSectionId);
            ON_SCOPE_EXIT {
                evaluator->popSectionId();
            };

            for (size_t i = 0; i < this->m_templateParameters.size(); i++) {
                auto &templateParameter = this->m_templateParameters[i];
                if(i >= templateArguments.size()) {

                    break;
                }
                if (!templateParameter->isType()) {
                    auto& argument = templateArguments[i];
                    auto literal = dynamic_cast<ASTNodeLiteral*>(argument.get());
                    auto value = literal->getValue();
                    // Allow the evaluator to throw an error at the correct source location.
                    if (this->m_currTemplateParameterType == nullptr) {
                        this->m_currTemplateParameterType = std::make_unique<ASTNodeTypeApplication>(std::make_shared<ASTNodeBuiltinType>(Token::ValueType::Auto));
                    }

                    this->m_currTemplateParameterType->setLocation(templateParameter->getLocation());

                    auto variable = evaluator->createVariable(templateParameter->getName().get(), this->m_currTemplateParameterType.get(), value, false, value.isPattern(), true, true);
                    if (variable != nullptr) {
                        variable->setInitialized(false);
                        evaluator->setVariable(variable, value);

                        auto templateVariable = variable->clone();
                        templateVariable->setVisibility(ptrn::Visibility::Hidden);
                        templatePatterns.emplace_back(std::move(templateVariable));
                    }
                } else {
                    auto& argument = templateArguments[i];
                    evaluator->getTypeTemplateParameters().emplace_back(std::move(argument));
                }
            }
        }

        auto type = this->getType();

        type->createPatterns(evaluator, resultPatterns);

        for (auto &pattern : resultPatterns) {
            if (pattern == nullptr)
                continue;

            if (auto iterable = dynamic_cast<ptrn::IIterable *>(pattern.get()); iterable != nullptr) {
                auto scope = iterable->getEntries();
                std::move(templatePatterns.begin(), templatePatterns.end(), std::back_inserter(scope));

                evaluator->pushScope(pattern, scope);
                ON_SCOPE_EXIT { evaluator->popScope(); };
                applyTypeAttributes(evaluator, this, pattern);

                iterable->setEntries(scope);
                templatePatterns.clear();
            } else {
                applyTypeAttributes(evaluator, this, pattern);
            }

        }
    }

    void ASTNodeTypeDecl::addAttribute(std::unique_ptr<ASTNodeAttribute> &&attribute) {
        if(auto attributable = dynamic_cast<Attributable*>(this->m_type.get()); attributable != nullptr) {
            attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(dynamic_cast<ASTNodeAttribute*>(attribute->clone().release())));
        }
        Attributable::addAttribute(std::move(attribute));
    }

    const ASTNode* ASTNodeTypeDecl::getTypeDefinition(Evaluator *evaluator) const {
        if(m_type == nullptr)
            err::E0004.throwError(fmt::format("Cannot use incomplete type '{}' before it has been defined.", this->m_name), "Try defining this type further up in your code before trying to instantiate it.", this->getLocation());
        else if(auto typeApp = dynamic_cast<ASTNodeTypeApplication*>(m_type.get()); typeApp != nullptr)
            return typeApp->getTypeDefinition(evaluator);
        else
            return m_type.get();
    }

    [[nodiscard]] const std::string ASTNodeTypeDecl::getTypeName() const {
        if(this->isTemplateType()) {
            std::string typeName = this->m_name + "<";
            for(const auto& param : this->m_templateParameters) {
                typeName += param->getName().get() + ", ";
            }
            if(!this->m_templateParameters.empty()) {
                typeName = typeName.substr(0, typeName.size() - 2);
            }
            typeName += ">";
            return typeName;
        } else {
            return this->m_name;
        }
    }
}