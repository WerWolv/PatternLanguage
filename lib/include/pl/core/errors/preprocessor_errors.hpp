#pragma once

#include <pl/core/errors/error.hpp>

namespace pl::core::err {

    namespace {
        class PreprocessorError : public Error<u32> {
        public:
            PreprocessorError(u32 errorCode, std::string title) noexcept :
                    Error('M', errorCode, std::move(title)) { }
        };
    }

    const static inline PreprocessorError M0001(1, "Unterminated comment");
    const static inline PreprocessorError M0002(2, "Unknown preprocessor directive");
    const static inline PreprocessorError M0003(3, "Invalid use of preprocessor directive");
    const static inline PreprocessorError M0004(4, "No such file or directory");
    const static inline PreprocessorError M0005(5, "File I/O error");
    const static inline PreprocessorError M0006(6, "Pragma usage error");
    const static inline PreprocessorError M0007(7, "#error directive");

}