#include <pl/core/preprocessor.hpp>

#include <fmt/format.h>
#include <pl/helpers/file.hpp>

namespace pl::core {

    Preprocessor::Preprocessor() {
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    Preprocessor::Preprocessor(const Preprocessor &other) {
        this->m_defines = other.m_defines;
        this->m_pragmas = other.m_pragmas;
        this->m_onceIncludedFiles = other.m_onceIncludedFiles;
        this->m_includePaths = other.m_includePaths;
        this->m_onlyIncludeOnce = false;
        this->m_pragmaHandlers = other.m_pragmaHandlers;

        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    std::optional<std::string> Preprocessor::preprocess(PatternLanguage &runtime, std::string code, bool initialRun) {
        u32 offset      = 0;
        u32 lineNumber  = 1;
        bool isInString = false;

        if (initialRun) {
            this->m_defines.clear();
            this->m_pragmas.clear();
        }

        std::string output;
        output.reserve(code.size());

        try {
            bool startOfLine = true;
            while (offset < code.length()) {
                if (code.substr(offset, 2) == "//") {
                    while (code[offset] != '\n' && offset < code.length())
                        offset += 1;
                } else if (code.substr(offset, 2) == "/*") {
                    auto commentStartLine = lineNumber;
                    while (code.substr(offset, 2) != "*/" && offset < code.length()) {
                        if (code[offset] == '\n') {
                            output += '\n';
                            lineNumber++;
                            startOfLine = true;
                        }

                        offset += 1;
                    }

                    offset += 2;
                    if (offset >= code.length())
                        err::M0001.throwError("Unterminated multiline comment. Expected closing */ sequence.", {}, commentStartLine);
                }

                if (offset > 0 && code[offset - 1] != '\\' && code[offset] == '\"')
                    isInString = !isInString;
                else if (isInString) {
                    output += code[offset];
                    offset += 1;
                    continue;
                }

                if (code[offset] == '#' && startOfLine) {
                    offset += 1;

                    if (code.substr(offset, 7) == "include") {
                        offset += 7;

                        while (std::isblank(code[offset]) || std::isspace(code[offset]))
                            offset += 1;

                        if (code[offset] != '<' && code[offset] != '"')
                            err::M0003.throwError("#include expects \"path/to/file\" or <path/to/file>.");

                        char endChar = code[offset];
                        if (endChar == '<') endChar = '>';

                        offset += 1;

                        std::string includeFile;
                        while (code[offset] != endChar) {
                            includeFile += code[offset];
                            offset += 1;

                            if (offset >= code.length() || code[offset] == '\n')
                                err::M0003.throwError(fmt::format("Missing terminating '{0}' character.", endChar));
                        }
                        offset += 1;

                        std::fs::path includePath = includeFile;

                        if (includeFile[0] != '/') {
                            for (const auto &dir : this->m_includePaths) {
                                std::fs::path tempPath = dir / includePath;
                                if (hlp::fs::isRegularFile(tempPath)) {
                                    includePath = tempPath;
                                    break;
                                }
                            }
                        }

                        if (!hlp::fs::isRegularFile(includePath)) {
                            if (includePath.parent_path().filename().string() == "std")
                                err::M0004.throwError("Path doesn't point to a valid file.", "This file might be part of the standard library. Make sure it's installed");
                            else
                                err::M0004.throwError("Path doesn't point to a valid file.");
                        }

                        hlp::fs::File file(includePath, hlp::fs::File::Mode::Read);
                        if (!file.isValid()) {
                            err::M0005.throwError(fmt::format("Failed to open file.", includeFile.c_str()));
                        }

                        Preprocessor preprocessor(*this);

                        auto preprocessedInclude = preprocessor.preprocess(runtime, file.readString(), /*initialRun =*/false);

                        if (!preprocessedInclude.has_value()) {
                            throw err::PatternLanguageError(*preprocessor.m_error);
                        }

                        std::copy(preprocessor.m_onceIncludedFiles.begin(), preprocessor.m_onceIncludedFiles.end(), std::inserter(this->m_onceIncludedFiles, this->m_onceIncludedFiles.begin()));
                        std::copy(preprocessor.m_defines.begin(), preprocessor.m_defines.end(), std::inserter(this->m_defines, this->m_defines.begin()));
                        std::copy(preprocessor.m_pragmas.begin(), preprocessor.m_pragmas.end(), std::inserter(this->m_pragmas, this->m_pragmas.begin()));

                        bool shouldInclude = true;
                        if (preprocessor.shouldOnlyIncludeOnce()) {
                            auto [iter, added] = this->m_onceIncludedFiles.insert(includePath);
                            if (!added) {
                                shouldInclude = false;
                            }
                        }

                        if (shouldInclude) {
                            auto content = preprocessedInclude.value();

                            std::replace(content.begin(), content.end(), '\n', ' ');
                            std::replace(content.begin(), content.end(), '\r', ' ');

                            output += content;
                        }
                    } else if (code.substr(offset, 6) == "define") {
                        offset += 6;

                        while (std::isblank(code[offset])) {
                            offset += 1;
                        }

                        std::string defineName;
                        while (!std::isblank(code[offset])) {
                            defineName += code[offset];

                            if (offset >= code.length() || code[offset] == '\n' || code[offset] == '\r')
                                err::M0003.throwError("No name given in #define directive.", "A #define directive expects a name and a value in the form of #define NAME VALUE");
                            offset += 1;
                        }

                        while (std::isblank(code[offset])) {
                            offset += 1;
                            if (offset >= code.length())
                                err::M0003.throwError("No value given in #define directive.", "A #define directive expects a name and a value in the form of #define <name> <value>");
                        }

                        std::string replaceValue;
                        while (code[offset] != '\n' && code[offset] != '\r') {
                            if (offset >= code.length())
                                err::M0003.throwError("Missing new line after preprocessor directive.");

                            replaceValue += code[offset];
                            offset += 1;
                        }

                        if (replaceValue.empty())
                            err::M0003.throwError("No value given in #define directive.", "A #define directive expects a name and a value in the form of #define <name> <value>.");

                        this->m_defines.emplace(defineName, replaceValue, lineNumber);
                    } else if (code.substr(offset, 6) == "pragma") {
                        offset += 6;

                        while (std::isblank(code[offset])) {
                            offset += 1;

                            if (code[offset] == '\n' || code[offset] == '\r')
                                err::M0003.throwError("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");
                        }

                        std::string pragmaKey;
                        while (!std::isblank(code[offset]) && code[offset] != '\n' && code[offset] != '\r') {
                            pragmaKey += code[offset];

                            if (offset >= code.length())
                                err::M0003.throwError("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");

                            offset += 1;
                        }

                        while (std::isblank(code[offset]))
                            offset += 1;

                        std::string pragmaValue;
                        while (code[offset] != '\n' && code[offset] != '\r') {
                            if (offset >= code.length())
                                err::M0003.throwError("Missing new line after preprocessor directive.");

                            pragmaValue += code[offset];
                            offset += 1;
                        }

                        this->m_pragmas.emplace(pragmaKey, pragmaValue, lineNumber);
                    } else {
                        err::M0002.throwError("Expected 'include', 'define' or 'pragma'");
                    }
                }

                if (code[offset] == '\n') {
                    lineNumber++;
                    startOfLine = true;
                } else if (!std::isspace(code[offset]))
                    startOfLine = false;

                output += code[offset];
                offset += 1;
            }

            // Apply defines
            std::vector<std::tuple<std::string, std::string, u32>> sortedDefines;
            std::copy(this->m_defines.begin(), this->m_defines.end(), std::back_inserter(sortedDefines));
            std::sort(sortedDefines.begin(), sortedDefines.end(), [](const auto &left, const auto &right) {
                return std::get<0>(left).size() > std::get<0>(right).size();
            });

            for (const auto &[define, value, defineLine] : sortedDefines) {
                size_t index = 0;
                while ((index = output.find(define, index)) != std::string::npos) {
                    output.replace(index, define.length(), value);
                    index += value.length();
                }
            }

            // Handle pragmas
            for (const auto &[type, value, pragmaLine] : this->m_pragmas) {
                if (this->m_pragmaHandlers.contains(type)) {
                    if (!this->m_pragmaHandlers[type](runtime, value))
                        err::M0006.throwError(fmt::format("Value '{}' cannot be used with the '{}' pragma directive.", value, type), { }, pragmaLine);
                } else
                    err::M0006.throwError(fmt::format("Pragma instruction '{}' does not exist.", type), { }, pragmaLine);
            }
        } catch (err::PreprocessorError::Exception &e) {
            auto line = e.getUserData() == 0 ? lineNumber : e.getUserData();
            this->m_error = err::PatternLanguageError(e.format(code, line, 1), line, 1);

            return std::nullopt;
        } catch (err::PatternLanguageError &e) {
            this->m_error = e;

            return std::nullopt;
        }

        return output;
    }

    void Preprocessor::addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler) {
        this->m_pragmaHandlers[pragmaType] = handler;
    }

    void Preprocessor::removePragmaHandler(const std::string &pragmaType) {
        this->m_pragmaHandlers.erase(pragmaType);
    }

}