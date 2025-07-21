#include <pl/core/ast/ast_node_function_definition.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_literal.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>

namespace pl::core::ast {

    ASTNodeFunctionDefinition::ASTNodeFunctionDefinition(std::string name, std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> &&params, std::vector<std::unique_ptr<ASTNode>> &&body, std::optional<std::string> parameterPack, std::vector<std::unique_ptr<ASTNode>> &&defaultParameters)
        : m_name(std::move(name)), m_params(std::move(params)), m_body(std::move(body)), m_parameterPack(std::move(parameterPack)), m_defaultParameters(std::move(defaultParameters)) { }

    ASTNodeFunctionDefinition::ASTNodeFunctionDefinition(const ASTNodeFunctionDefinition &other) : ASTNode(other) {
        this->m_name = other.m_name;
        this->m_parameterPack = other.m_parameterPack;

        for (const auto &[name, type] : other.m_params) {
            this->m_params.emplace_back(name, type->clone());
        }

        for (auto &statement : other.m_body) {
            this->m_body.push_back(statement->clone());
        }

        for (auto &param : other.m_defaultParameters) {
            this->m_defaultParameters.push_back(param->clone());
        }
    }


    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeFunctionDefinition::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        api::FunctionParameterCount paramCount;

        if (this->m_parameterPack.has_value() && !this->m_defaultParameters.empty())
            paramCount = api::FunctionParameterCount::atLeast(this->m_params.size() - this->m_defaultParameters.size());
        else if (this->m_parameterPack.has_value())
            paramCount = api::FunctionParameterCount::atLeast(this->m_params.size());
        else if (!this->m_defaultParameters.empty())
            paramCount = api::FunctionParameterCount::between(this->m_params.size() - this->m_defaultParameters.size(), this->m_params.size());
        else
            paramCount = api::FunctionParameterCount::exactly(this->m_params.size());

        std::vector<Token::Literal> evaluatedDefaultParams;
        for (const auto &param : this->m_defaultParameters) {
            const auto expression = param->evaluate(evaluator)->evaluate(evaluator);

            if (auto literal = dynamic_cast<ASTNodeLiteral *>(expression.get()); literal != nullptr) {
                evaluatedDefaultParams.push_back(literal->getValue());
            } else {
                err::E0009.throwError("Default value must be a literal.", {}, this->getLocation());
            }
        }

        evaluator->addCustomFunction(this->m_name, paramCount, evaluatedDefaultParams, [this](Evaluator *ctx, const std::vector<Token::Literal> &params) -> std::optional<Token::Literal> {
            std::vector<std::shared_ptr<ptrn::Pattern>> variables;

            auto startOffset = ctx->getBitwiseReadOffset();
            ctx->pushScope(nullptr, variables);
            ctx->pushSectionId(ptrn::Pattern::HeapSectionId);
            ON_SCOPE_EXIT {
                ctx->popScope();
                ctx->setBitwiseReadOffset(startOffset);
                ctx->popSectionId();
            };

            if (this->m_parameterPack.has_value()) {
                std::vector<Token::Literal> parameterPackContent;
                for (u32 paramIndex = this->m_params.size(); paramIndex < params.size(); paramIndex++)
                    parameterPackContent.push_back(params[paramIndex]);

                ctx->createParameterPack(this->m_parameterPack.value(), parameterPackContent);
            }

            std::vector<std::pair<std::shared_ptr<ptrn::Pattern>, std::string>> originalNames;
            ON_SCOPE_EXIT {
                for (auto &[variable, name] : originalNames) {
                    variable->setVariableName(name, variable->getVariableLocation());
                }
            };
            for (u32 paramIndex = 0; paramIndex < this->m_params.size() && paramIndex < params.size(); paramIndex++) {
                const auto &[name, type] = this->m_params[paramIndex];

                if (auto typeNode = dynamic_cast<ASTNodeTypeDecl *>(type.get()); typeNode != nullptr) {
                    bool reference = typeNode->isReference();

                    if (params[paramIndex].isString())
                        reference = false;

                    auto variable = ctx->createVariable(name, type->getLocation(), typeNode, params[paramIndex], false, reference);

                    if (reference && params[paramIndex].isPattern()) {
                        auto pattern = params[paramIndex].toPattern();
                        variable->setSection(pattern->getSection());
                        variable->setOffset(pattern->getOffset());
                        variable = pattern;
                    }

                    ctx->setVariable(name, params[paramIndex]);
                    originalNames.emplace_back(variable, name);

                    variable->setVariableName(name, type->getLocation());

                    ctx->setCurrentControlFlowStatement(ControlFlowStatement::None);
                }
            }

            for (auto &statement : this->getBody()) {
                auto result = statement->execute(ctx);

                if (ctx->getCurrentControlFlowStatement() != ControlFlowStatement::None) {
                    switch (ctx->getCurrentControlFlowStatement()) {
                        case ControlFlowStatement::Break:
                            err::E0010.throwError("Break statements can only be used within a loop.", {}, this->getLocation());
                        case ControlFlowStatement::Continue:
                            err::E0010.throwError("Continue statements can only be used within a loop.", {}, this->getLocation());
                        default:
                            break;
                    }

                    ctx->setCurrentControlFlowStatement(ControlFlowStatement::None);

                    if (!result.has_value())
                        return std::nullopt;
                    else
                        return std::visit(wolv::util::overloaded {
                                [](const auto &value) -> FunctionResult {
                                    return value;
                                },
                                [ctx](const std::shared_ptr<ptrn::Pattern> &pattern) -> FunctionResult {
                                    auto &prevScope = ctx->getScope(-1);
                                    auto &currScope = ctx->getScope(0);

                                    prevScope.heapStartSize = currScope.heapStartSize = ctx->getHeap().size();

                                    return pattern;
                                }
                        }, result.value());
                }
            }

            return {};
        });

        return nullptr;
    }

}