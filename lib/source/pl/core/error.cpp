#include <pl/core/errors/error.hpp>
#include <pl/api.hpp>

#include <pl/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

#include <fmt/format.h>

namespace pl::core::err::impl {

    std::string formatLocation(Location location) {
        if (location.line > 0 && location.column > 0) {
            return fmt::format("{}:{}:{}", location.source->source, location.line, location.column);
        }
        return "";
    }

    std::string formatLines(Location location) {
        std::string result;

        if (const auto lines = wolv::util::splitString(location.source->content, "\n");
                location.line - 1 < lines.size()) {
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

            result += fmt::format("{}{}\n", lineNumberPrefix, errorLine);

            {
                const auto arrowSpacing = std::string(lineNumberPrefix.length() + arrowPosition, ' ');
                // add arrow with length of the token
                result += arrowSpacing + std::string(location.length, '^') + '\n';
                // add spacing for the error message
                result += arrowSpacing + std::string(location.length, ' ');
            }
        }

        return result;
    }

    std::string formatRuntimeErrorShort(
        char prefix,
        const std::string &title,
        const std::string &description)
    {
        return fmt::format("runtime error: {}\n{}", prefix, title, description);
    }

    std::string formatRuntimeError(
            const Location& location,
            char prefix,
            const std::string &title,
            const std::string &description,
            const std::string &hint)
    {
        std::string errorMessage;

        errorMessage += fmt::format("runtime error: {}\n", prefix, title);

        if (location.line > 0 && location.column > 0) {
            errorMessage += formatLocation(location) + description + '\n';
        } else {
            errorMessage += description + '\n';
        }

        if (!hint.empty()) {
            errorMessage += fmt::format("hint: {}", hint);
        }

        return errorMessage;
    }

    std::string formatCompilerError(
            const Location& location,
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
            errorMessage += formatLines(location) + "\n";
        }

        if(!description.empty()) {
            errorMessage += "\n" + description + "\n";
        }

        return errorMessage;
    }

}