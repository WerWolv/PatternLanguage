#include <pl.hpp>
#include <pl/token.hpp>
#include <pl/log_console.hpp>
#include <pl/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <vector>
#include <string>

namespace pl::libstd::mem {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        api::Namespace nsStdMem = { "builtin", "std", "mem" };
        {

            /* base_address() */
            runtime.addFunction(nsStdMem, "base_address", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                pl::unused(params);

                return u128(ctx->getDataBaseAddress());
            });

            /* size() */
            runtime.addFunction(nsStdMem, "size", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                pl::unused(params);

                return u128(ctx->getDataSize());
            });

            /* find_sequence_in_range(occurrence_index, start_offset, end_offset, bytes...) */
            runtime.addFunction(nsStdMem, "find_sequence_in_range", FunctionParameterCount::moreThan(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto occurrenceIndex = Token::literalToUnsigned(params[0]);
                auto offsetFrom      = Token::literalToUnsigned(params[1]);
                auto offsetTo        = Token::literalToUnsigned(params[2]);

                std::vector<u8> sequence;
                for (u32 i = 3; i < params.size(); i++) {
                    auto byte = Token::literalToUnsigned(params[i]);

                    if (byte > 0xFF)
                        LogConsole::abortEvaluation(fmt::format("byte #{} value out of range: {} > 0xFF", i, u64(byte)));

                    sequence.push_back(u8(byte & 0xFF));
                }

                std::vector<u8> bytes(sequence.size(), 0x00);
                u32 occurrences      = 0;
                const u64 bufferSize = ctx->getDataSize();
                const u64 endOffset  = offsetTo <= offsetFrom ? bufferSize : std::min(bufferSize, u64(offsetTo));
                for (u64 offset = offsetFrom; offset < endOffset - sequence.size(); offset++) {
                    ctx->readData(offset, bytes.data(), bytes.size());

                    if (bytes == sequence) {
                        if (occurrences < occurrenceIndex) {
                            occurrences++;
                            continue;
                        }

                        return u128(offset);
                    }
                }

                return i128(-1);
            });

            /* read_unsigned(address, size) */
            runtime.addFunction(nsStdMem, "read_unsigned", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size    = Token::literalToUnsigned(params[1]);

                if (size > 16)
                    LogConsole::abortEvaluation("read size out of range");

                u128 result = 0;
                ctx->readData(address, &result, size);

                return result;
            });

            /* read_signed(address, size) */
            runtime.addFunction(nsStdMem, "read_signed", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size    = Token::literalToUnsigned(params[1]);

                if (size > 16)
                    LogConsole::abortEvaluation("read size out of range");

                i128 value;
                ctx->readData(address, &value, size);
                return pl::signExtend(size * 8, value);
            });

            /* read_string(address, size) */
            runtime.addFunction(nsStdMem, "read_string", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size    = Token::literalToUnsigned(params[1]);

                std::string result(size, '\x00');
                ctx->readData(address, result.data(), size);

                return result;
            });
        }
    }

}