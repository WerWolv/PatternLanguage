#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/hash/crc.hpp>

namespace pl::lib::libstd::hash {

    template<size_t Size>
    static u128 crc(pl::core::Evaluator *ctx, const auto &params) {
        if (!params[0].isPattern() && !params[0].isString())
            core::err::E0012.throwError("Only patterns and strings are supported for CRC hash functions.");

        auto bytes   = params[0].toBytes();
        auto init    = params[1].toUnsigned();
        auto poly    = params[2].toUnsigned();
        auto xorout  = params[3].toUnsigned();
        auto reflectIn  = params[4].toUnsigned();
        auto reflectOut = params[5].toUnsigned();

        wolv::hash::Crc<Size> crc(poly, init, xorout, reflectIn, reflectOut);
        crc.process(bytes);

        return u128(crc.getResult());
    }

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdHash = { "builtin", "std", "hash" };
        {
            /* crc8(pattern, init, poly, xorout, reflect_in, reflect_out) */
            runtime.addFunction(nsStdHash, "crc8", FunctionParameterCount::exactly(6), [](Evaluator *ctx, const auto &params) -> std::optional<Token::Literal> {
                wolv::util::unused(ctx);

                return crc<8>(params);
            });

            /* crc16(pattern, init, poly, xorout, reflect_in, reflect_out) */
            runtime.addFunction(nsStdHash, "crc16", FunctionParameterCount::exactly(6), [](Evaluator *ctx, const auto &params) -> std::optional<Token::Literal> {
                wolv::util::unused(ctx);

                return crc<16>(params);
            });

            /* crc32(pattern, init, poly, xorout, reflect_in, reflect_out) */
            runtime.addFunction(nsStdHash, "crc32", FunctionParameterCount::exactly(6), [](Evaluator *ctx, const auto &params) -> std::optional<Token::Literal> {
                wolv::util::unused(ctx);

                return crc<32>(params);
            });

            /* crc64(pattern, init, poly, xorout, reflect_in, reflect_out) */
            runtime.addFunction(nsStdHash, "crc64", FunctionParameterCount::exactly(6), [](Evaluator *ctx, const auto &params) -> std::optional<Token::Literal> {
                wolv::util::unused(ctx);

                return crc<64>(params);
            });
        }
    }

}