#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/lib/std/types.hpp>

#include <vector>
#include <string>


namespace pl::lib::libstd::mem {

    static std::optional<i128> findSequence(::pl::core::Evaluator *ctx, u64 occurrenceIndex, u64 offsetFrom, u64 offsetTo, const std::vector<u8> &sequence) {
        u32 occurrences = 0;
        const u64 bufferSize = ctx->getDataSize();

        if (offsetFrom >= offsetTo || sequence.empty() || bufferSize == 0)
            return std::nullopt;

        if (offsetTo - offsetFrom > bufferSize)
            offsetTo = offsetFrom + bufferSize;

        std::vector<u8> bytes(std::max(sequence.size(), size_t(4 * 1024)), 0x00);
        for (u64 offset = offsetFrom; offset < offsetTo; offset += bytes.size()) {
            const auto bytesToRead = std::min<std::size_t>(bytes.size(), offsetTo - offset);
            ctx->readData(offset, bytes.data(), bytesToRead, ptrn::Pattern::MainSectionId);
            ctx->handleAbort();

            for (u64 i = 0; i < bytes.size(); i += 1) {
                if (bytes[i] == sequence[0]) [[unlikely]] {
                    bool found = true;
                    for (u64 j = 1; j < sequence.size(); j++) {
                        if (bytes[i + j] != sequence[j]) {
                            found = false;
                            break;
                        }
                    }

                    if (found) [[unlikely]] {
                        if (occurrences >= occurrenceIndex)
                            return offset + i;

                        occurrences++;
                    }
                }
            }
        }

        return std::nullopt;
    };

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        api::Namespace nsStdMem = { "builtin", "std", "mem" };
        {

            /* base_address() */
            runtime.addFunction(nsStdMem, "base_address", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                wolv::util::unused(params);

                return u128(ctx->getDataBaseAddress());
            });

            /* size() */
            runtime.addFunction(nsStdMem, "size", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                wolv::util::unused(params);

                return u128(ctx->getDataSize());
            });

            /* find_sequence_in_range(occurrence_index, start_offset, end_offset, bytes...) */
            runtime.addFunction(nsStdMem, "find_sequence_in_range", FunctionParameterCount::moreThan(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto occurrenceIndex = u64(params[0].toUnsigned());
                auto offsetFrom      = u64(params[1].toUnsigned());
                auto offsetTo        = u64(params[2].toUnsigned());

                std::vector<u8> sequence;
                for (u32 i = 3; i < params.size(); i++) {
                    auto byte = params[i].toUnsigned();

                    if (byte > 0xFF)
                        err::E0012.throwError(fmt::format("Invalid byte value {:x}.", byte), "Try a value between 0x00 and 0xFF.");

                    sequence.push_back(u8(byte));
                }

                return findSequence(ctx, occurrenceIndex, offsetFrom, offsetTo, sequence).value_or(-1);
            });

            /* find_string_in_range(occurrence_index, start_offset, end_offset, string) */
            runtime.addFunction(nsStdMem, "find_string_in_range", FunctionParameterCount::exactly(4), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto occurrenceIndex = u64(params[0].toUnsigned());
                auto offsetFrom      = u64(params[1].toUnsigned());
                auto offsetTo        = u64(params[2].toUnsigned());
                auto string          = params[3].toString(false);

                return findSequence(ctx, occurrenceIndex, offsetFrom, offsetTo, std::vector<u8>(string.data(), string.data() + string.size())).value_or(-1);
            });

            /* read_unsigned(address, size, endian) */
            runtime.addFunction(nsStdMem, "read_unsigned", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address            = u64(params[0].toUnsigned());
                auto size               = size_t(params[1].toSigned());
                types::Endian endian    = params[2].toUnsigned();

                if (size < 1 || size > 16)
                    err::E0012.throwError(fmt::format("Read size {} is out of range.", size), "Try a value between 1 and 16.");

                u128 result = 0;
                ctx->readData(address, &result, size, ptrn::Pattern::MainSectionId);
                result = hlp::changeEndianess(result, size, endian);

                return result;
            });

            /* read_signed(address, size, endian) */
            runtime.addFunction(nsStdMem, "read_signed", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address            = u64(params[0].toUnsigned());
                auto size               = size_t(params[1].toSigned());
                types::Endian endian    = params[2].toUnsigned();

                if (size < 1 || size > 16)
                    err::E0012.throwError(fmt::format("Read size {} is out of range.", size), "Try a value between 1 and 16.");


                i128 value = 0;
                ctx->readData(address, &value, size, ptrn::Pattern::MainSectionId);
                value = hlp::changeEndianess(value, size, endian);

                return hlp::signExtend(size * 8, value);
            });

            /* read_string(address, size, endian) */
            runtime.addFunction(nsStdMem, "read_string", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto address = u64(params[0].toUnsigned());
                auto size    = size_t(params[1].toUnsigned());

                std::string result(size, '\x00');
                ctx->readData(address, result.data(), size, ptrn::Pattern::MainSectionId);

                return result;
            });


            /* current_bit_offset() */
            runtime.addFunction(nsStdMem, "current_bit_offset", FunctionParameterCount::none(), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                wolv::util::unused(params);
                return u128(ctx->getBitwiseReadOffset().bitOffset);
            });

            /* read_bits(byteOffset, bitOffset, bitSize) */
            runtime.addFunction(nsStdMem, "read_bits", FunctionParameterCount::exactly(3), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto byteOffset = params[0].toUnsigned();
                auto bitOffset = u8(params[1].toUnsigned());
                auto bitSize = u64(params[2].toUnsigned());
                return ctx->readBits(byteOffset, bitOffset, bitSize, ptrn::Pattern::MainSectionId, ctx->getDefaultEndian());
            });


            /* create_section(name) -> id */
            runtime.addFunction(nsStdMem, "create_section", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto name = params[0].toString(false);

                return u128(ctx->createSection(name));
            });

            /* delete_section(id) */
            runtime.addFunction(nsStdMem, "delete_section", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto id = u64(params[0].toUnsigned());

                ctx->removeSection(id);

                return std::nullopt;
            });

            /* get_section_size(id) -> size */
            runtime.addFunction(nsStdMem, "get_section_size", FunctionParameterCount::exactly(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto id = u64(params[0].toUnsigned());

                return u128(ctx->getSection(id).size());
            });

            /* set_section_size(id, size) */
            runtime.addFunction(nsStdMem, "set_section_size", FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto id   = u64(params[0].toUnsigned());
                auto size = size_t(params[1].toUnsigned());

                ctx->getSection(id).resize(size);

                return std::nullopt;
            });

            /* copy_section_to_section(from_id, from_address, to_id, to_address, size) */
            runtime.addFunction(nsStdMem, "copy_to_section", FunctionParameterCount::exactly(5), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
                auto fromId     = u64(params[0].toUnsigned());
                auto fromAddr   = u64(params[1].toUnsigned());
                auto toId       = u64(params[2].toUnsigned());
                auto toAddr     = u64(params[3].toUnsigned());
                auto size       = size_t(params[4].toUnsigned());

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
                auto toId       = u64(params[1].toUnsigned());
                auto toAddr     = u64(params[2].toUnsigned());

                if (toId == ptrn::Pattern::MainSectionId)
                    err::E0012.throwError("Cannot write to main section.", "The main section represents the currently loaded data and is immutable.");
                else if (toId == ptrn::Pattern::HeapSectionId)
                    err::E0012.throwError("Invalid section id.");

                auto& section = ctx->getSection(toId);

                switch (params[0].getType()) {
                    using enum Token::ValueType;
                    case String: {
                        auto string = params[0].toString(false);

                        if (section.size() < toAddr + string.size())
                            section.resize(toAddr + string.size());

                        std::copy(string.begin(), string.end(), section.begin() + toAddr);
                        break;
                    }
                    case CustomType: {
                        auto pattern = params[0].toPattern();

                        if (section.size() < toAddr + pattern->getSize())
                            section.resize(toAddr + pattern->getSize());

                        if (auto iterable = dynamic_cast<ptrn::IIterable*>(pattern.get())) {
                            iterable->forEachEntry(0, iterable->getEntryCount(), [&](u64, ptrn::Pattern *entry) {
                                auto entrySize = entry->getSize();
                                ctx->readData(entry->getOffset(), section.data() + toAddr, entrySize, entry->getSection());
                                toAddr += entrySize;
                            });
                        } else {
                            ctx->readData(pattern->getOffset(), section.data() + toAddr, pattern->getSize(), pattern->getSection());
                        }
                        break;
                    }
                    default:
                        err::E0012.throwError("Invalid value type.", "Only strings and patterns are allowed.");
                }

                return std::nullopt;
            });
        }
    }

}