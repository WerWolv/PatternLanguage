#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/lib/std/types.hpp>

#include <vector>
#include <string>


namespace pl::lib::libstd::mem {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdMem = { "builtin", "std", "mem" };
        {

            /* base_address() */
            runtime.addFunction(nsStdMem, "base_address", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                hlp::unused(params);

                return u128(ctx->getDataBaseAddress());
            });

            /* size() */
            runtime.addFunction(nsStdMem, "size", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                hlp::unused(params);

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
                        err::E0012.throwError(fmt::format("Invalid byte value {}.", byte), "Try a value between 0x00 and 0xFF.");

                    sequence.push_back(u8(byte & 0xFF));
                }

                std::vector<u8> bytes(sequence.size(), 0x00);
                u32 occurrences      = 0;
                const u64 bufferSize = ctx->getDataSize();
                const u64 endOffset  = offsetTo <= offsetFrom ? bufferSize : std::min(bufferSize, u64(offsetTo));
                for (u64 offset = offsetFrom; offset < endOffset - sequence.size(); offset++) {
                    ctx->readData(offset, bytes.data(), bytes.size(), ptrn::Pattern::MainSectionId);

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
            runtime.addFunction(nsStdMem, "read_unsigned", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address            = Token::literalToUnsigned(params[0]);
                auto size               = Token::literalToSigned(params[1]);
                types::Endian endian    = Token::literalToUnsigned(params[2]);

                if (size < 1 || size > 16)
                    err::E0012.throwError(fmt::format("Read size {} is out of range.", size), "Try a value between 1 and 16.");

                u128 result = 0;
                ctx->readData(address, &result, size, ptrn::Pattern::MainSectionId);
                result = hlp::changeEndianess(result, size, endian);

                return result;
            });

            /* read_signed(address, size) */
            runtime.addFunction(nsStdMem, "read_signed", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address            = Token::literalToUnsigned(params[0]);
                auto size               = Token::literalToSigned(params[1]);
                types::Endian endian    = Token::literalToUnsigned(params[2]);

                if (size < 1 || size > 16)
                    err::E0012.throwError(fmt::format("Read size {} is out of range.", size), "Try a value between 1 and 16.");


                i128 value = 0;
                ctx->readData(address, &value, size, ptrn::Pattern::MainSectionId);
                value = hlp::changeEndianess(value, size, endian);

                return hlp::signExtend(size * 8, value);
            });

            /* read_string(address, size) */
            runtime.addFunction(nsStdMem, "read_string", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = Token::literalToUnsigned(params[0]);
                auto size    = Token::literalToUnsigned(params[1]);

                std::string result(size, '\x00');
                ctx->readData(address, result.data(), size, ptrn::Pattern::MainSectionId);

                return result;
            });


            /* create_section(name) -> id */
            runtime.addFunction(nsStdMem, "create_section", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto name = Token::literalToString(params[0], false);

                return u128(ctx->createSection(name));
            });

            /* delete_section(id) */
            runtime.addFunction(nsStdMem, "delete_section", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto id = Token::literalToUnsigned(params[0]);

                ctx->removeSection(id);

                return std::nullopt;
            });

            /* get_section_size(id) -> size */
            runtime.addFunction(nsStdMem, "get_section_size", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto id = Token::literalToUnsigned(params[0]);

                return u128(ctx->getSection(id).size());
            });

            /* copy_section_to_section(from_id, from_address, to_id, to_address, size) */
            runtime.addFunction(nsStdMem, "copy_to_section", FunctionParameterCount::exactly(5), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto fromId     = Token::literalToUnsigned(params[0]);
                auto fromAddr   = Token::literalToUnsigned(params[1]);
                auto toId       = Token::literalToUnsigned(params[2]);
                auto toAddr     = Token::literalToUnsigned(params[3]);
                auto size       = Token::literalToUnsigned(params[4]);

                std::vector<u8> data(size, 0x00);
                ctx->readData(fromAddr, data.data(), size, fromId);
                if (toId == ptrn::Pattern::MainSectionId)
                    err::E0012.throwError("Cannot write to main section.", "The main section represents the currently loaded data and is immutable.");
                else if (toId == ptrn::Pattern::HeapSectionId)
                    err::E0012.throwError("Invalid section id.");

                auto& section = ctx->getSection(toId);
                if (section.size() < toAddr + size)
                    section.resize(toAddr + size);
                std::memcpy(section.data() + toAddr, data.data(), size);

                return std::nullopt;
            });

            /* copy_value_to_section(value, section_id, to_address) */
            runtime.addFunction(nsStdMem, "copy_value_to_section", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto value      = Token::literalToPattern(params[0]);
                auto toId       = Token::literalToUnsigned(params[1]);
                auto toAddr     = Token::literalToUnsigned(params[2]);

                if (toId == ptrn::Pattern::MainSectionId)
                    err::E0012.throwError("Cannot write to main section.", "The main section represents the currently loaded data and is immutable.");
                else if (toId == ptrn::Pattern::HeapSectionId)
                    err::E0012.throwError("Invalid section id.");

                auto size = value->getSize();
                auto& section = ctx->getSection(toId);
                if (section.size() < toAddr + size)
                    section.resize(toAddr + size);

                if (auto iterable = dynamic_cast<ptrn::Iteratable*>(value)) {
                    iterable->forEachEntry(0, iterable->getEntryCount(), [&](u64, ptrn::Pattern *entry) {
                        auto entrySize = entry->getSize();
                        ctx->readData(entry->getOffset(), section.data() + toAddr, entrySize, entry->getSection());
                        toAddr += entrySize;
                    });
                } else {
                    ctx->readData(value->getOffset(), section.data() + toAddr, size, value->getSection());
                }

                return std::nullopt;
            });
        }
    }

}