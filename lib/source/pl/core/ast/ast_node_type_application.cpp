#include <pl/core/ast/ast_node_type_appilication.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    static std::string computeTemplateTypeString(const std::vector<std::unique_ptr<ASTNode>> &arguments) {
        std::string templateTypeString;
        for (size_t i = 0; i < arguments.size(); i++) {
            auto &templateArgument = arguments[i];
            if (auto literal = dynamic_cast<ASTNodeLiteral*>(templateArgument.get()); literal != nullptr) {
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
            } else if (auto typeNode = dynamic_cast<ASTNodeTypeApplication*>(templateArgument.get()); typeNode != nullptr) {
                templateTypeString += fmt::format("{}, ", typeNode->getTypeName());
            }
        }

        return templateTypeString.size() > 2 ? templateTypeString.substr(0, templateTypeString.size() - 2) : templateTypeString;
    }
    
    ASTNodeTypeApplication::ASTNodeTypeApplication(std::shared_ptr<ASTNode> type)
        : m_type(std::move(type)) { };

    ASTNodeTypeApplication::ASTNodeTypeApplication(const ASTNodeTypeApplication &other)
        : ASTNode(other), m_type(other.m_type) {
        for (const auto &arg : other.m_templateArguments)
            this->m_templateArguments.emplace_back(arg->clone());
    };

    std::vector<std::unique_ptr<ASTNode>> ASTNodeTypeApplication::evaluateTemplateArguments(Evaluator *evaluator) const
    {
        std::vector<std::unique_ptr<ASTNode>> templateArgs(this->m_templateArguments.size());
        std::string templateTypeString;
        for (size_t i = 0; i < this->m_templateArguments.size(); i++) {
            auto &templateArgument = this->m_templateArguments[i];
            templateArgs[i] = templateArgument->evaluate(evaluator);
        }
    
        return templateArgs;
    }


    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeTypeApplication::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);
        auto evaluatedTemplateArguments = this->evaluateTemplateArguments(evaluator);
        auto evaluatedType = std::make_unique<ASTNodeTypeApplication>(this->m_type);
        if(this->m_type == nullptr) {
            auto& templateTypeParameters = evaluator->getTypeTemplateParameters();
            if(this->m_templateParameterIndex >= templateTypeParameters.size()) {
                err::E0004.throwError("Template parameter index out of bounds.", {}, this->getLocation());
            } else {
                evaluatedType->m_type = templateTypeParameters[this->m_templateParameterIndex];
            }   
        } else {
            evaluatedType->m_type = this->m_type;
        }

        evaluatedType->m_templateArguments = std::move(evaluatedTemplateArguments);
        evaluatedType->setLocation(this->getLocation());
        evaluatedType->setShouldDocument(this->shouldDocument());
        evaluatedType->setDocComment(this->getDocComment());
        return evaluatedType;
    }

    void ASTNodeTypeApplication::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);
        auto actualType = this->m_type;
        if(actualType == nullptr) {
            auto& templateTypeParameters = evaluator->getTypeTemplateParameters();
            if(this->m_templateParameterIndex >= templateTypeParameters.size()) {
                err::E0004.throwError("Template parameter index out of bounds.", {}, this->getLocation());
            }
            actualType = templateTypeParameters[this->m_templateParameterIndex];

            if(actualType.get() == this) {
                err::E0004.throwError("Recursive type definition detected.", {}, this->getLocation());
            }
        }
    
        auto templateArgs = this->evaluateTemplateArguments(evaluator);
        auto templateTypeString = computeTemplateTypeString(templateArgs);
        evaluator->setCurrentTemplateArguments(std::move(templateArgs));
        auto currEndian = evaluator->getDefaultEndian();
        ON_SCOPE_EXIT { evaluator->setDefaultEndian(currEndian); };

        evaluator->setDefaultEndian(this->m_endian.value_or(currEndian));
        actualType->createPatterns(evaluator, resultPatterns);
        for(auto& pattern : resultPatterns) {
            if (!pattern->hasOverriddenEndian())
                pattern->setEndian(evaluator->getDefaultEndian());
            if(auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(actualType.get()); typeDecl != nullptr) {
                if (!typeDecl->getName().empty()) {
                    if(this->m_templateArguments.empty()) {
                        pattern->setTypeName(typeDecl->getName());
                    } else {
                        pattern->setTypeName(fmt::format("{}<{}>", typeDecl->getName(), templateTypeString));
                    }
                }
            }
        }
    }

    const ast::ASTNode* ASTNodeTypeApplication::getTypeDefinition(Evaluator *evaluator) const{
        if(this->m_type == nullptr) {
            auto& templateTypeParameters = evaluator->getTypeTemplateParameters();
            if(this->m_templateParameterIndex >= templateTypeParameters.size()) {
                err::E0004.throwError("Template parameter index out of bounds.", {}, this->getLocation());
            }
            auto type = templateTypeParameters[this->m_templateParameterIndex].get();
            if(auto typeApp = dynamic_cast<ASTNodeTypeApplication*>(type); typeApp != nullptr) {
                return typeApp->getTypeDefinition(evaluator);
            }
        }

        ast::ASTNode* type = this->getType().get();
        if (auto typDecl = dynamic_cast<ast::ASTNodeTypeDecl*>(type); typDecl != nullptr) {
            return typDecl->getTypeDefinition(evaluator);
        } else if(auto builtinType = dynamic_cast<ast::ASTNodeBuiltinType*>(type); builtinType != nullptr) {
            return builtinType;
        }
    
        return nullptr;
    }

    [[nodiscard]] const std::string ASTNodeTypeApplication::getTypeName() const {
        if(auto typeDecl = dynamic_cast<const ASTNodeTypeDecl*>(this->m_type.get()); typeDecl != nullptr) {
            std::string typeName = typeDecl->getName();
            if(this->m_templateArguments.empty()) {
                return typeName;
            } else {
                return fmt::format("{}<{}>", typeName, computeTemplateTypeString(this->m_templateArguments));
            }
        } else if(auto builtinType = dynamic_cast<const ASTNodeBuiltinType*>(this->m_type.get()); builtinType != nullptr) {
            return Token::getTypeName(builtinType->getType());
        } else if(auto typeApp = dynamic_cast<const ASTNodeTypeApplication*>(this->m_type.get()); typeApp != nullptr) {
            return typeApp->getTypeName();
        } else {
            return "????";
        }
    }
}