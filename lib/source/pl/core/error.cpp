#include <pl/core/errors/error.hpp>
#include <pl/api.hpp>

#include <pl/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

#include <fmt/format.h>

#include <iostream>

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

    std::string formatLocation(Location location) {
        if (location.line > 0 && location.column > 0) {
            return fmt::format("{}:{}:{}", location.source->source, location.line, location.column);
        }
        return "";
    }

    std::string formatImpl(
            Location location,
            const std::string& message,
            const std::string& description,
            const std::vector<Location>& trace) {
        std::string errorMessage = "error: " + message + "\n";

        if (location.line > 0 &&location. column > 0) {
            errorMessage += "  -->   in " + formatLocation(location) + "\n";
        }

        for (const auto &traceLocation: trace) {
            errorMessage += "   >> from " + formatLocation(traceLocation) + "\n";
        }

        if (location.line > 0 && location.column > 0) {

            auto lines = wolv::util::splitString(location.source->content, "\n");

            std::cout << "location.line: " << location.line << std::endl;
            std::cout << "location.column: " << location.column << std::endl;
            std::cout << "location.source->source: " << location.source->source << std::endl;
            std::cout << "location.source->content: " << std::endl;

            for (const auto &item: lines) {
                std::cout << item << std::endl;
            }

            if ((location.line - 1) < lines.size()) {
                const auto &errorLine = lines[location.line - 1];
                const auto lineNumberPrefix = fmt::format("{} | ", location.line);
                errorMessage += fmt::format("{}{}", lineNumberPrefix, errorLine);

                {
                    const auto descriptionSpacing = std::string(lineNumberPrefix.length() + location.column - 1, ' ');
                    errorMessage += descriptionSpacing + "^\n";
                    errorMessage += descriptionSpacing + description + "\n\n";
                }
            }
        } else {
            errorMessage += description + "\n";
        }

        return errorMessage;
    }

}