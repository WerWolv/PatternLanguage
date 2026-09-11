#pragma once

#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <pl/helpers/types.hpp>
#include <pl/core/errors/error.hpp>

#include <pl/core/errors/result.hpp>

#include <pl/core/ast/visitors/ast_validator.hpp>

namespace pl::core {

    namespace ast { class ASTNode; }

    class ValidatorPipeline {
    public:
        using Result = hlp::CompileResult<bool>;

        ValidatorPipeline() = default;

        template <std::derived_from<ASTValidator>... Vs>
        ValidatorPipeline(std::unique_ptr<Vs>... validators) {
            (add(std::move(validators)), ...);
        }

        void add(std::unique_ptr<ASTValidator> validator);

        Result validate(const std::vector<std::shared_ptr<ast::ASTNode>> &ast);

    private:
        std::vector<std::unique_ptr<ASTValidator>> m_validators;
    };
}
