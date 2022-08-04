#include <pl/errors/error.hpp>

namespace pl::err {

    namespace {
        class LexerError : public Error {
        public:
            LexerError(u32 errorCode, std::string title) noexcept :
                    Error('L', errorCode, std::move(title)) { }
        };
    }

    const static inline LexerError L0001(1, "Invalid character literal");
    const static inline LexerError L0002(2, "Invalid string literal");
    const static inline LexerError L0003(3, "Invalid integer literal");
    const static inline LexerError L0004(4, "Unknown sequence");

}