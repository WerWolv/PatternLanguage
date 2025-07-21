#include <pl/core/ast/ast_node_function_call.hpp>

#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <pl/core/ast/ast_node_parameter_pack.hpp>
#include <pl/core/ast/ast_node_mathematical_expression.hpp>
#include <pl/core/ast/ast_node_literal.hpp>

namespace pl::core::ast {

    ASTNodeFunctionCall::ASTNodeFunctionCall(std::string functionName, std::vector<std::unique_ptr<ASTNode>> &&params)
    : ASTNode(), m_functionName(std::move(functionName)), m_params(std::move(params)) { }

    ASTNodeFunctionCall::ASTNodeFunctionCall(const ASTNodeFunctionCall &other) : ASTNode(other) {
        this->m_functionName = other.m_functionName;

        for (auto &param : other.m_params)
            this->m_params.push_back(param->clone());
    }

    void ASTNodeFunctionCall::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &) const {
        this->execute(evaluator);
    }

    [[nodiscard]] std::unique_ptr<ASTNode> ASTNodeFunctionCall::evaluate(Evaluator *evaluator) const {
        [[maybe_unused]] auto context = evaluator->updateRuntime(this);

        evaluator->pushSectionId(ptrn::Pattern::HeapSectionId);

        auto startOffset = evaluator->getBitwiseReadOffset();
        ON_SCOPE_EXIT {
            evaluator->setBitwiseReadOffset(startOffset);
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
                evaluator->getConsole().log(LogConsole::Level::Warning, "This function might be part of the standard library.\nYou can install the standard library though\nthe Content Store found under Extras -> Content Store and then\ninclude the correct file.");
            }

            err::E0003.throwError(fmt::format("Cannot call unknown function '{}'.", functionName), fmt::format("Try defining it first using 'fn {}() {{ }}'", functionName), this->getLocation());
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
            err::E0009.throwError(fmt::format("Too few parameters passed to function '{0}'. Expected at least {1} but got {2}.", functionName, min, evaluatedParams.size()), { }, this->getLocation());
        else if (evaluatedParams.size() > max)
            err::E0009.throwError(fmt::format("Too many parameters passed to function '{0}'. Expected {1} but got {2}.", functionName, max, evaluatedParams.size()), { }, this->getLocation());

        if (evaluator->isDebugModeEnabled())
            evaluator->getConsole().log(LogConsole::Level::Debug, fmt::format("Calling function {}({}).", functionName, [&]{
                std::string parameters;
                for (const auto &param : evaluatedParams)
                    parameters += fmt::format("{}, ", param.toString(true));

                if (!evaluatedParams.empty())
                    parameters.resize(parameters.size() - 2);

                return parameters;
            }()));

        std::vector<std::pair<ptrn::Pattern*, std::string>> variables;
        ON_SCOPE_EXIT {
                          for (size_t i = 0; i < variables.size(); i++) {
                              auto &[pattern, name] = variables[i];

                              pattern->setVariableName(name, pattern->getVariableLocation());
                          }
                      };

        for (auto &param : evaluatedParams) {
            std::visit(wolv::util::overloaded {
                    [](auto &) {},
                    [&](std::shared_ptr<ptrn::Pattern> &pattern) {
                        variables.push_back({ pattern.get(), pattern->getVariableName() });
                    }
            }, param);
        }

        auto controlFlow = evaluator->getCurrentControlFlowStatement();
        ON_SCOPE_EXIT {
            evaluator->setCurrentControlFlowStatement(controlFlow);
        };
        auto result = function->func(evaluator, evaluatedParams);

        if (result.has_value())
            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(result.value())));
        else
            return std::unique_ptr<ASTNode>(new ASTNodeMathematicalExpression(nullptr, nullptr, Token::Operator::Plus));
    }

    ASTNode::FunctionResult ASTNodeFunctionCall::execute(Evaluator *evaluator) const {
        (void)this->evaluate(evaluator);

        return {};
    }

}