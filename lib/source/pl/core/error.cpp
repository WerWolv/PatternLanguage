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

            if ((location.line - 1) < lines.size()) {
                const auto lineNumberPrefix = fmt::format("{} | ", location.line);
                auto errorLine = wolv::util::replaceStrings(lines[location.line - 1], "\r", "");
                u32 arrowPosition = location.column - 1;

                // trim to size
                if(errorLine.size() > 40) { // shrink to [column - 20; column + 20]
                    const auto column = location.column - 1;
                    auto start = column > 20 ? column - 20 : 0;
                    auto end = column + 20 < errorLine.size() ? column + 20 : errorLine.size();
                    // search for whitespaces on both sides until a maxium of 10 characters and change start/end accordingly
                    for(auto i = 0; i < 10; ++i) {
                        if(start > 0 && errorLine[start] != ' ') {
                            --start;
                        }
                        if(end < errorLine.size() && errorLine[end] != ' ') {
                            ++end;
                        }
                    }
                    errorLine = errorLine.substr(start, end - start);
                    arrowPosition = column - start - 1;
                }

                errorMessage += fmt::format("{}{}\n", lineNumberPrefix, errorLine);

                {
                    const auto descriptionSpacing = std::string(lineNumberPrefix.length() + arrowPosition, ' ');
                    errorMessage += descriptionSpacing + "^\n";
                }
            }
        }

        if(!description.empty()) {
            errorMessage += "\n" + description + "\n";
        }

        return errorMessage;
    }

}