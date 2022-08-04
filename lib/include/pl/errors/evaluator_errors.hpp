#include <pl/errors/error.hpp>

namespace pl::err {

    namespace {
        class EvaluatorError : public Error {
        public:
            EvaluatorError(u32 errorCode, std::string title) noexcept :
                    Error('E', errorCode, std::move(title)) { }
        };
    }

    const static inline EvaluatorError E0001(1, "Invalid parser result");

}