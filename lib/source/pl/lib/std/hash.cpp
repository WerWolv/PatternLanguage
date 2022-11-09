#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <ctime>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace pl::lib::libstd::hash {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdHash = { "builtin", "std", "hash" };
        {
            /* crc32(pattern, init, poly) */
            runtime.addFunction(nsStdHash, "crc32", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto pattern = Token::literalToPattern(params[0]);
                auto init    = Token::literalToUnsigned(params[1]);
                auto poly    = Token::literalToUnsigned(params[2]);

                // Lookup table generation
                const auto table = [&] {
                    std::array<u32, 256> table = { 0 };

                    for (u16 i = 0; i < 256; i++) {
                        u32 c = i;
                        for (u8 j = 0; j < 8; j++) {
                            if (c & 1) c = poly ^ (c >> 1);
                            else c >>= 1;
                        }
                        table[i] = c;
                    }

                    return table;
                }();

                u32 crc = init;
                u8 byte = 0x00;
                for (u64 i = 0; i < pattern->getSize(); i++) {
                    ctx->readData(pattern->getOffset() + i, &byte, sizeof(byte), pattern->getSection());
                    crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
                }

                return u128(~crc);
            });
        }
    }

}