#include <numeric>

#include <pl.hpp>


namespace pl::lib::libstd::crypto {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdMath = { "builtin", "std", "crypto" };
        {
            /* xor(buffer, key) */
            runtime.addFunction(nsStdMath, "xor", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                if (!params[0].isPattern() && !params[0].isString())
                    pl::core::err::E0012.throwError("Only patterns and strings are supported for crypto::crc buffer.");

                if (!params[1].isPattern() && !params[1].isString())
                    pl::core::err::E0012.throwError("Only patterns and strings are supported for crypto::crc key.");

                auto buffer = params[0].toBytes();
                auto key    = params[1].toBytes();

                auto result = buffer;

                for (std::size_t i = 0; i < result.size(); ++i)
                    result[i] ^= key[i % key.size()];

                return std::string(result.begin(), result.end());
            });

        }
    }

}