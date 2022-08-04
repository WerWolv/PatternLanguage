#include <pl/errors/error.hpp>

namespace pl::err {

    namespace {
        class ValidatorError : public Error {
        public:
            ValidatorError(u32 errorCode, std::string title) noexcept :
                    Error('V', errorCode, std::move(title)) { }
        };
    }

    const static inline ValidatorError V0001(1, "Invalid parser result");
    const static inline ValidatorError V0002(2, "Statement redefinition");

}