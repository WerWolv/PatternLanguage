#pragma once

#include <pl/core/errors/error.hpp>

namespace pl::core::err {

    namespace {
        class ParserError : public Error<u32> {
        public:
            ParserError(u32 errorCode, std::string title) noexcept :
                    Error('P', errorCode, std::move(title)) { }
        };
    }

    const static inline ParserError P0001(1, "Invalid token");
    const static inline ParserError P0002(2, "Unexpected expression");
    const static inline ParserError P0003(3, "Unknown type");
    const static inline ParserError P0004(4, "Scope resolution error");
    const static inline ParserError P0005(5, "Invalid type operator argument");
    const static inline ParserError P0006(6, "Invalid type cast");
    const static inline ParserError P0007(7, "Attribute applied to invalid statement");
    const static inline ParserError P0008(8, "Invalid function definition");
    const static inline ParserError P0009(9, "Invalid pointer definition");
    const static inline ParserError P0010(10, "Invalid In/Out variable type");
    const static inline ParserError P0011(11, "Type redefinition");

}