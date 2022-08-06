#pragma once

#include <helpers/types.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pl/errors/validator_errors.hpp>

namespace pl {

    class ASTNode;

    class Validator {
    public:
        Validator() = default;

        bool validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ASTNode>> &ast);

        const std::optional<err::PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<err::PatternLanguageError> m_error;
    };

}