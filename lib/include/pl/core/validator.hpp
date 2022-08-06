#pragma once


#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pl/helpers/types.hpp>
#include <pl/core/errors/validator_errors.hpp>

namespace pl::core {

    namespace ast { class ASTNode; }

    class Validator {
    public:
        Validator() = default;

        bool validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ast::ASTNode>> &ast);

        const std::optional<err::PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<err::PatternLanguageError> m_error;
    };

}