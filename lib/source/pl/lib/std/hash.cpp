#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/hash/crc.hpp>

namespace pl::lib::libstd::hash {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdHash = { "builtin", "std", "hash" };
        {
            /* crc32(pattern, init, poly, xorout, reflect_in, reflect_out) */
            runtime.addFunction(nsStdHash, "crc32", FunctionParameterCount::exactly(6), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                wolv::util::unused(ctx);

                auto pattern = params[0].toPattern();
                auto init    = params[1].toUnsigned();
                auto poly    = params[2].toUnsigned();
                auto xorout  = params[3].toUnsigned();
                auto reflectIn  = params[4].toUnsigned();
                auto reflectOut = params[5].toUnsigned();

                wolv::hash::Crc<32> crc(poly, init, xorout, reflectIn, reflectOut);
                crc.process(pattern->getBytes());

                return u128(crc.getResult());
            });
        }
    }

}