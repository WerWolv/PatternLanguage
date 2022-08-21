#pragma once

#include <pl/core/errors/error.hpp>

namespace pl::core::err {

    namespace {
        class ValidatorError : public Error<> {
        public:
            ValidatorError(u32 errorCode, std::string title) noexcept :
                    Error('V', errorCode, std::move(title)) { }
        };
    }

    const static inline ValidatorError V0001(1, "Invalid parser result");
    const static inline ValidatorError V0002(2, "Statement redefinition");
    const static inline ValidatorError V0003(3, "Recursion error");

}