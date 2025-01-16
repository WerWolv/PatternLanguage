#include <pl/core/errors/error.hpp>
#include <pl/api.hpp>

#include <wolv/utils/string.hpp>

namespace pl::core::err::impl {

    std::string formatLocation(Location location, std::optional<u64> address) {
        if (location.line > 0 && location.column > 0) {
            if (address.has_value())
                return fmt::format("{}:{}:{} at address 0x{:04X}", location.source->source, location.line, location.column, address.value());
            else
                return fmt::format("{}:{}:{}", location.source->source, location.line, location.column);
        }
        return "";
    }

    std::string formatLines(Location location) {
        std::string result;

        const auto lines = wolv::util::splitString(location.source->content, "\n");

        if (location.line < lines.size() + 1) {
            const auto lineNumberPrefix = fmt::format("{} | ", location.line);
            auto errorLine = wolv::util::replaceStrings(lines[location.line - 1], "\r", "");
            u32 arrowPosition = location.column - 1;

            // Trim to size
            if (errorLine.length() > 40) { // shrink to [column - 20; column + 20]
                const auto column = location.column;
                auto start = column > 20 ? column - 20 : 0;
                auto end = column + 20 < errorLine.length() ? column + 20 : errorLine.length();
                // Search for whitespaces on both sides until a maximum of 10 characters and change start/end accordingly
                for(auto i = 0; i < 10; ++i) {
                    if(start > 0 && errorLine[start] != ' ') {
                        --start;
                    }
                    if(end < errorLine.length() && errorLine[end] != ' ') {
                        ++end;
                    }
                }
                errorLine = errorLine.substr(start, end - start);
                arrowPosition = column - start;
            }

            result += fmt::format("{}{}\n", lineNumberPrefix, errorLine);

            {
                const auto arrowSpacing = std::string(lineNumberPrefix.length() + arrowPosition, ' ');
                // Add arrow with length of the token
                result += arrowSpacing;
                result += std::string(location.length, '^');
            }
        }

        return result;
    }

    std::string formatRuntimeErrorShort(
        const std::string &message,
        const std::string &description)
    {
        if(description.empty())
            return fmt::format("runtime error: {}", message);

        return fmt::format("runtime error: {}\n{}", message, description);
    }

    std::string formatRuntimeError(
            const Location& location,
            const std::string &message,
            const std::string &description,
            std::optional<u64> address)
    {
        std::string errorMessage = "runtime error: " + message + "\n";

        if (location.line > 0) {
            errorMessage += "  -->   in " + formatLocation(location, address) + "\n";
        }

        if (location.line > 0) {
            errorMessage += formatLines(location);
        }

        if (!description.empty()) {
            errorMessage += "\n\n" + description;
        }

        return errorMessage;
    }

    std::string formatCompilerError(
            const Location& location,
            const std::string& message,
            const std::string& description,
            const std::vector<Location>& trace) {
        std::string errorMessage = "error: " + message + "\n";

        if (location.line > 0) {
            errorMessage += "  -->   in " + formatLocation(location) + "\n";
        }

        for (const auto &traceLocation : trace) {
            errorMessage += "   >> from " + formatLocation(traceLocation) + "\n";
        }

        if (location.line > 0) {
            errorMessage += formatLines(location);
        }

        if (!description.empty()) {
            errorMessage += "\n\n" + description;
        }

        return errorMessage;
    }

}