#include <pl/core/validator.hpp>

#include <pl/core/ast/ast_node.hpp>

#include <unordered_set>
#include <string>

namespace pl::core {
    void ValidatorPipeline::add(std::unique_ptr<pl::core::ASTValidator> validator) {
        m_validators.push_back(std::move(validator));
    }

    ValidatorPipeline::Result ValidatorPipeline::validate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast) {
        using ValidatorErrors = std::vector<err::CompileError>;

        std::vector<ValidatorErrors> pipeline_errors;
        size_t errorsCount = 0;

        for (auto& validator: m_validators) {
            auto [_, errors] = validator->validate(ast);
            errorsCount += errors.size();
            pipeline_errors.push_back(std::move(errors));
        }

        if (errorsCount == 0) {
            return { };
        }
        // We have some errors

        std::vector<err::CompileError> result;
        result.reserve(errorsCount);
        for (auto& errors: pipeline_errors) {
            std::move(errors.begin(), errors.end(), std::back_inserter(result));
        }
        return Result::err(std::move(result));
    }
}