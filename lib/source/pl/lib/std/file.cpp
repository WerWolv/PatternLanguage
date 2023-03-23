#include <pl.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/io/file.hpp>

namespace pl::lib::libstd::file {

    void registerFunctions(pl::PatternLanguage &runtime) {
        using FunctionParameterCount = pl::api::FunctionParameterCount;
        using namespace pl::core;

        const static auto throwInvalidFileError = []{
            err::E0001.throwError("Failed to access invalid file.", {}, nullptr);
        };

        api::Namespace nsStdFile = { "builtin", "std", "file" };
        {
            static u32 fileCounter = 0;
            static std::map<u32, wolv::io::File> openFiles;

            runtime.addCleanupCallback([](pl::PatternLanguage&) {
                for (auto &[id, file] : openFiles)
                    file.close();

                openFiles.clear();
                fileCounter = 0;
            });

            /* open(path, mode) */
            runtime.addDangerousFunction(nsStdFile, "open", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto path     = params[0].toString(false);
                const auto modeEnum = params[1].toUnsigned();

                wolv::io::File::Mode mode;
                switch (modeEnum) {
                    case 1:
                        mode = wolv::io::File::Mode::Read;
                        break;
                    case 2:
                        mode = wolv::io::File::Mode::Write;
                        break;
                    case 3:
                        mode = wolv::io::File::Mode::Create;
                        break;
                    default:
                        err::E0012.throwError("Invalid file open mode.", "Try 'std::fs::Mode::Read', 'std::fs::Mode::Write' or 'std::fs::Mode::Create'.");
                }

                wolv::io::File file(path, mode);

                if (!file.isValid())
                    err::E0012.throwError(fmt::format("Failed to open file '{}'.", path));

                fileCounter++;
                openFiles.emplace(std::pair { fileCounter, std::move(file) });

                return u128(fileCounter);
            });

            /* close(file) */
            runtime.addDangerousFunction(nsStdFile, "close", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles.erase(file);

                return std::nullopt;
            });

            /* read(file, size) */
            runtime.addDangerousFunction(nsStdFile, "read", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();
                const auto size = params[1].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                auto buffer = openFiles[file].readVector(size);

                return std::string(buffer.begin(), buffer.end());
            });

            /* write(file, data) */
            runtime.addDangerousFunction(nsStdFile, "write", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();
                const auto data = params[1].toString(true);

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].writeString(data);

                return std::nullopt;
            });

            /* seek(file, offset) */
            runtime.addDangerousFunction(nsStdFile, "seek", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();
                const auto offset = params[1].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].seek(offset);

                return std::nullopt;
            });

            /* size(file) */
            runtime.addDangerousFunction(nsStdFile, "size", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                return u128(openFiles[file].getSize());
            });

            /* resize(file, size) */
            runtime.addDangerousFunction(nsStdFile, "resize", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();
                const auto size = params[1].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].setSize(size);

                return std::nullopt;
            });

            /* flush(file) */
            runtime.addDangerousFunction(nsStdFile, "flush", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].flush();

                return std::nullopt;
            });

            /* remove(file) */
            runtime.addDangerousFunction(nsStdFile, "remove", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto file = params[0].toUnsigned();

                if (!openFiles.contains(file))
                    throwInvalidFileError();

                openFiles[file].remove();

                return std::nullopt;
            });
        }
    }

}