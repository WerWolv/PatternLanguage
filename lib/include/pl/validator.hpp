#pragma once

#include <helpers/types.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pl/error.hpp>
#include <pl/errors/validator_errors.hpp>

namespace pl {

    class ASTNode;

    class Validator {
    public:
        Validator() = default;

        bool validate(const std::string &sourceCode, const std::vector<std::shared_ptr<ASTNode>> &ast);

        const std::optional<err::Error::Exception> &getError() { return this->m_error; }

    private:
        std::optional<err::Error::Exception> m_error;
    };

}