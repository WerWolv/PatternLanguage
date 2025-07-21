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
            err::E0004.throwError(fmt::format("Cannot use incomplete type '{}' before it has been defined.", this->m_name), "Try defining this type further up in your code before trying to instantiate it.", this->getLocation());

        return this->m_type;
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeTypeDecl::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

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

    void ASTNodeTypeDecl::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        std::vector<std::unique_ptr<ASTNodeLiteral>> templateParamLiterals(this->m_templateParameters.size());

        // Get template parameter values before pushing a new template parameter scope so that variables from the parent scope are used before being overwritten.
        std::string templateTypeString;
        for (size_t i = 0; i < this->m_templateParameters.size(); i++) {
            auto &templateParameter = this->m_templateParameters[i];
            if (auto lvalue = dynamic_cast<ASTNodeLValueAssignment *>(templateParameter.get())) {
                if (!lvalue->getRValue())
                    err::E0003.throwError(fmt::format("No value set for non-type template parameter {}. This is a bug.", lvalue->getLValueName()), {}, this->getLocation());
                auto valueNode = lvalue->getRValue()->evaluate(evaluator);
                if (auto literal = dynamic_cast<ASTNodeLiteral*>(valueNode.get()); literal != nullptr) {
                    const auto &value = literal->getValue();

                    if (value.isString()) {
                        auto string = value.toString();
                        if (string.size() > 32)
                            string = "...";
                        templateTypeString += fmt::format("\"{}\", ", hlp::encodeByteString({ string.begin(), string.end() }));
                    }
                    else if (value.isPattern()) {
                        templateTypeString += fmt::format("{}{{ }}, ", value.toPattern()->getTypeName());
                    }
                    else {
                        templateTypeString += fmt::format("{}, ", value.toString(true));
                    }

                    templateParamLiterals[i] = std::make_unique<ASTNodeLiteral>(value);
                } else {
                    err::E0003.throwError(fmt::format("Template parameter {} is not a literal. This is a bug.", lvalue->getLValueName()), {}, this->getLocation());
                }
            } else if (const auto *typeNode = dynamic_cast<ASTNodeTypeDecl*>(templateParameter.get())) {
                const ASTNode *node = typeNode->getType().get();
                while (node != nullptr) {
                    if (const auto *innerNode = dynamic_cast<const ASTNodeTypeDecl*>(node)) {
                        if (const auto name = innerNode->getName(); !name.empty()) {
                            templateTypeString += fmt::format("{}, ", name);
                            break;
                        }
                        node = innerNode->getType().get();
                    }
                    if (const auto *innerNode = dynamic_cast<const ASTNodeBuiltinType*>(node)) {
                        templateTypeString += fmt::format("{}, ", Token::getTypeName(innerNode->getType()));

                        break;
                    }
                }
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

                    this->m_currTemplateParameterType->setLocation(lvalue->getLocation());

                    auto variable = evaluator->createVariable(lvalue->getLValueName(), lvalue->getLocation(), this->m_currTemplateParameterType.get(), value, false, value.isPattern(), true, true);
                    if (variable != nullptr) {
                        variable->setInitialized(false);
                        evaluator->setVariable(variable, value);

                        auto templateVariable = variable->clone();
                        templateVariable->setVisibility(ptrn::Visibility::Hidden);
                        templatePatterns.emplace_back(std::move(templateVariable));
                    }
                }
            }
        }

        auto currEndian = evaluator->getDefaultEndian();
        ON_SCOPE_EXIT { evaluator->setDefaultEndian(currEndian); };

        evaluator->setDefaultEndian(this->m_endian.value_or(currEndian));
        this->getType()->createPatterns(evaluator, resultPatterns);

        for (auto &pattern : resultPatterns) {
            if (pattern == nullptr)
                continue;

            if (!pattern->hasOverriddenEndian())
                pattern->setEndian(evaluator->getDefaultEndian());

            if (!this->m_name.empty()) {
                if (this->m_templateParameters.empty()) {
                    pattern->setTypeName(this->m_name);
                } else if (templateTypeString.size() >= 2) {
                    pattern->setTypeName(fmt::format("{}<{}>", this->m_name, templateTypeString.substr(0, templateTypeString.size() - 2)));
                }
            }

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
        if (this->isValid()) {
            if (auto attributable = dynamic_cast<Attributable *>(this->getType().get()); attributable != nullptr) {
                attributable->addAttribute(std::unique_ptr<ASTNodeAttribute>(static_cast<ASTNodeAttribute *>(attribute->clone().release())));
            }
        }

        Attributable::addAttribute(std::move(attribute));
    }

}