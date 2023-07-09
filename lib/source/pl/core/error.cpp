#include <pl/core/errors/error.hpp>

#include <pl/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

#include <fmt/format.h>

namespace pl::core::err {

    std::string formatShortImpl(
            char prefix,
            u32 errorCode,
            const std::string &title,
            const std::string &description)
    {
        return fmt::format("error[{}{:04}]: {}\n{}", prefix, errorCode, title, description);
    }

    std::string formatImpl(
            const std::string &sourceCode,
            u32 line, u32 column,
            char prefix,
            u32 errorCode,
            const std::string &title,
            const std::string &description,
            const std::string &hint)
    {
        std::string errorMessage;

        errorMessage += fmt::format("error[{}{:04}]: {}\n", prefix, errorCode, title);

        if (line > 0 && column > 0) {
            errorMessage += fmt::format("  --> <Source Code>:{}:{}\n", line, column);

            auto lines = wolv::util::splitString(sourceCode, "\n");

            if ((line - 1) < lines.size()) {
                const auto &errorLine = lines[line - 1];
                const auto lineNumberPrefix = fmt::format("{} | ", line);
                errorMessage += fmt::format("{}{}\n", lineNumberPrefix, errorLine);

                {
                    const auto descriptionSpacing = std::string(lineNumberPrefix.length() + column - 1, ' ');
                    errorMessage += descriptionSpacing + "^\n";
                    errorMessage += descriptionSpacing + description + "\n\n";
                }
            }
        } else {
            errorMessage += description + "\n";
        }

        if (!hint.empty()) {
            errorMessage += fmt::format("hint: {}", hint);
        }

        return errorMessage;
    }

}