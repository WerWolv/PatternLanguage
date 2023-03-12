#pragma once

#include <pl/core/ast/ast_node.hpp>
#include <pl/core/ast/ast_node_parameter_pack.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

#include <thread>

namespace pl::core::ast {

    class ASTNodeFunctionCall : public ASTNode {
    public:
        explicit ASTNodeFunctionCall(std::string functionName, std::vector<std::unique_ptr<ASTNode>> &&params)
            : ASTNode(), m_functionName(std::move(functionName)), m_params(std::move(params)) { }

        ASTNodeFunctionCall(const ASTNodeFunctionCall &other) : ASTNode(other) {
            this->m_functionName = other.m_functionName;

            for (auto &param : other.m_params)
                this->m_params.push_back(param->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeFunctionCall(*this));
        }

        [[nodiscard]] const std::string &getFunctionName() const {
            return this->m_functionName;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getParams() const {
            return this->m_params;
        }

        [[nodiscard]] std::vector<std::shared_ptr<ptrn::Pattern>> createPatterns(Evaluator *evaluator) const override {
            this->execute(evaluator);

            return {};
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            evaluator->updateRuntime(this);

            evaluator->pushSectionId(ptrn::Pattern::HeapSectionId);

            auto startOffset = evaluator->dataOffset();
            ON_SCOPE_EXIT {
                evaluator->dataOffset() = startOffset;
                evaluator->popSectionId();
            };

            std::vector<Token::Literal> evaluatedParams;
            for (auto &param : this->getParams()) {
                const auto expression = param->evaluate(evaluator)->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(expression.get()); literal != nullptr) {
                    evaluatedParams.push_back(literal->getValue());
                } else if (auto parameterPack = dynamic_cast<ASTNodeParameterPack *>(expression.get())) {
                    for (auto &value : parameterPack->getValues()) {
                        evaluatedParams.push_back(value);
                    }
                }
            }

            const auto &functionName = this->getFunctionName();
            auto function = evaluator->findFunction(functionName);

            if (!function.has_value()) {
                if (functionName.starts_with("std::")) {
                    evaluator->getConsole().log(LogConsole::Level::Warning, "This function might be part of the standard library.\nYou can install the standard library though\nthe Content Store found under Help -> Content Store and then\ninclude the correct file.");
                }

                err::E0003.throwError(fmt::format("Cannot call unknown function '{}'.", functionName), fmt::format("Try defining it first using 'fn {}() {{ }}'", functionName), this);
            }

            const auto &[min, max] = function->parameterCount;

            if (evaluatedParams.size() >= min && evaluatedParams.size() < max) {
                while (true) {
                    auto remainingParams = max - evaluatedParams.size();
                    if (remainingParams <= 0)
                        break;

                    auto offset = evaluatedParams.size() - min;
                    if (offset >= function->defaultParameters.size())
                        break;

                    evaluatedParams.push_back(function->defaultParameters[offset]);
                }
            }

            if (evaluatedParams.size() < min)
                err::E0009.throwError(fmt::format("Too few parameters passed to function '{0}'. Expected at least {1} but got {2}.", functionName, min, evaluatedParams.size()), { }, this);
            else if (evaluatedParams.size() > max)
                err::E0009.throwError(fmt::format("Too many parameters passed to function '{0}'. Expected {1} but got {2}.", functionName, max, evaluatedParams.size()), { }, this);

            if (function->dangerous && evaluator->getDangerousFunctionPermission() != DangerousFunctionPermission::Allow) {
                evaluator->dangerousFunctionCalled();

                if (evaluator->getDangerousFunctionPermission() == DangerousFunctionPermission::Deny) {
                    err::E0009.throwError(fmt::format("Call to dangerous function '{}' has been denied.", functionName), { }, this);
                }
            }

            if (evaluator->isDebugModeEnabled())
                evaluator->getConsole().log(LogConsole::Level::Debug, fmt::format("Calling function {}({}).", functionName, [&]{
                    std::string parameters;
                    for (const auto &param : evaluatedParams)
                        parameters += fmt::format("{}, ", param.toString(true));

                    if (!evaluatedParams.empty())
                        parameters.resize(parameters.size() - 2);

                    return parameters;
                }()));

            auto result = function->func(evaluator, evaluatedParams);

            if (result.has_value())
                return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(result.value())));
            else
                return std::unique_ptr<ASTNode>(new ASTNodeMathematicalExpression(nullptr, nullptr, Token::Operator::Plus));
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            (void)this->evaluate(evaluator);

            return {};
        }

    private:
        std::string m_functionName;
        std::vector<std::unique_ptr<ASTNode>> m_params;
    };

}