#include <pl.hpp>
#include <pl/token.hpp>
#include <pl/log_console.hpp>
#include <pl/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <helpers/file.hpp>

namespace pl::libstd::file {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        const static auto throwInvalidFileError = []{
            err::E0001.throwError("Failed to access invalid file.", {}, nullptr);
        };

        api::Namespace nsStdFile = { "builtin", "std", "file" };
        {
            static u32 fileCounter = 0;
            static std::map<u32, fs::File> openFiles;

            /* open(path, mode) */
            runtime.addDangerousFunction(nsStdFile, "open", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto path     = Token::literalToString(params[0], false);
                const auto modeEnum = Token::literalToUnsigned(params[1]);

                fs::File::Mode mode;
                switch (modeEnum) {
                    case 1:
                        mode = fs::File::Mode::Read;
                        break;
                    case 2:
                        mode = fs::File::Mode::Write;
                        break;
                    case 3:
                        mode = fs::File::Mode::Create;
                        break;
                    default:
                        err::E0012.throwError("Invalid file open mode.", "Try 'std::fs::Mode::Read', 'std::fs::Mode::Write' or 'std::fs::Mode::Create'.");
                }

                fs::File file(path, mode);

                if (!file.isValid())
                    err::E0012.throwError(fmt::format("Failed to open file '{}'.", path));

                fileCounter++;
                openFiles.emplace(std::pair { fileCounter, std::move(file) });

                return u128(fileCounter);
            });

            /* close(file) */
            runtime.addDangerousFunction(nsStdFile, "close", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles.erase(file);

                return std::nullopt;
            });

            /* read(file, size) */
            runtime.addDangerousFunction(nsStdFile, "read", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto size = Token::literalToUnsigned(params[1]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                return openFiles[file].readString(size);
            });

            /* write(file, data) */
            runtime.addDangerousFunction(nsStdFile, "write", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto data = Token::literalToString(params[1], true);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].write(data);

                return std::nullopt;
            });

            /* seek(file, offset) */
            runtime.addDangerousFunction(nsStdFile, "seek", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file   = Token::literalToUnsigned(params[0]);
                const auto offset = Token::literalToUnsigned(params[1]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].seek(offset);

                return std::nullopt;
            });

            /* size(file) */
            runtime.addDangerousFunction(nsStdFile, "size", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                return u128(openFiles[file].getSize());
            });

            /* resize(file, size) */
            runtime.addDangerousFunction(nsStdFile, "resize", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);
                const auto size = Token::literalToUnsigned(params[1]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].setSize(size);

                return std::nullopt;
            });

            /* flush(file) */
            runtime.addDangerousFunction(nsStdFile, "flush", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].flush();

                return std::nullopt;
            });

            /* remove(file) */
            runtime.addDangerousFunction(nsStdFile, "remove", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = Token::literalToUnsigned(params[0]);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].remove();

                return std::nullopt;
            });
        }
    }

}