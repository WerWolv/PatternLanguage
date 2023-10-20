#include <pl/core/preprocessor.hpp>
#include <pl/core/errors/preprocessor_errors.hpp>

#include <fmt/format.h>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace pl::core {

    Preprocessor::Preprocessor() : ErrorCollector() {
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    Preprocessor::Preprocessor(const Preprocessor &other) : ErrorCollector(other) {
        this->m_defines = other.m_defines;
        this->m_pragmas = other.m_pragmas;
        this->m_onceIncludedFiles = other.m_onceIncludedFiles;
        this->m_onlyIncludeOnce = false;
        this->m_pragmaHandlers = other.m_pragmaHandlers;

        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    CompileResult<std::string> Preprocessor::preprocess(PatternLanguage &runtime, const std::string &sourceCode, const std::string &source, bool initialRun) {
        m_offset      = 0;
        m_lineNumber  = 1;
        m_source      = source;
        bool isInString = false;

        std::vector<bool> ifDefs;

        std::string_view code = sourceCode;

        if (initialRun) {
            this->m_onceIncludedFiles.clear();
            this->m_onlyIncludeOnce = false;
            this->m_pragmas.clear();
        }

        auto getDirectiveValue = [&](bool allowWhitespace = false) -> std::optional<std::string> {
            while (std::isblank(code[m_offset])) {
                m_offset += 1;

                if (code[m_offset] == '\n' || code[m_offset] == '\r')
                    return std::nullopt;
            }

            std::string value;
            while ((allowWhitespace || !std::isblank(code[m_offset])) && code[m_offset] != '\n' && code[m_offset] != '\r') {
                value += code[m_offset];

                m_offset += 1;
                if (m_offset >= code.length())
                    break;
            }

            return wolv::util::trim(value);
        };

        auto getDirective = [&](std::string directive, bool hasArgs = true) {
            if (hasArgs)
                directive += " ";

            if (code.substr(m_offset, directive.length()) == directive) {
                m_offset += directive.length();
                return true;
            } else return false;
        };

        std::string output;
        output.reserve(sourceCode.size());

        bool startOfLine = true;
        while (m_offset < code.length()) {
            if (code.substr(m_offset, 2) == "//") {
                while (m_offset < code.length() && code[m_offset] != '\n')
                    m_offset += 1;

                if (code.length() == m_offset)
                    break;

            } else if ((code.substr(m_offset, 2) == "/*" && code.substr(m_offset, 3) != "/**" && code.substr(m_offset, 3) != "/*!") || (!initialRun && code.substr(m_offset, 3) == "/*!")) {
                auto commentStartLine = m_lineNumber;
                while (m_offset < code.length() && code.substr(m_offset, 2) != "*/") {
                    if (code[m_offset] == '\n') {
                        output += '\n';
                        m_lineNumber++;
                        m_lineBeginOffset = m_offset;
                        startOfLine = true;
                    }

                    m_offset += 1;
                }

                m_offset += 2;
                if (m_offset >= code.length())
                    err::M0001.throwError("Unterminated multiline comment. Expected closing */ sequence.", {}, commentStartLine);
            }

            if (ifDefs.empty() || ifDefs.back()) {
                if (m_offset > 0 && code[m_offset - 1] != '\\' && code[m_offset] == '\"')
                    isInString = !isInString;
                else if (isInString) {
                    output += code[m_offset];
                    m_offset += 1;
                    continue;
                }
            }

            if (code[m_offset] == '#' && startOfLine) {
                m_offset += 1;

                if (getDirective("ifdef")) {
                    auto defineName = getDirectiveValue();
                    if (!defineName.has_value())
                        err::M0003.throwError("No define name given to #ifdef directive.");

                    if (!ifDefs.empty() && !ifDefs.back())
                        ifDefs.push_back(false);
                    else
                        ifDefs.push_back(this->m_defines.contains(*defineName));
                } else if (getDirective("ifndef")) {
                    auto defineName = getDirectiveValue();
                    if (!defineName.has_value())
                        err::M0003.throwError("No define name given to #ifdef directive.");

                    if (!ifDefs.empty() && !ifDefs.back())
                        ifDefs.push_back(false);
                    else
                        ifDefs.push_back(!this->m_defines.contains(*defineName));
                } else if (getDirective("endif", false)) {
                     if (ifDefs.empty())
                         err::M0003.throwError("#endif without #ifdef.");
                     else
                         ifDefs.pop_back();
                } else if (ifDefs.empty() || ifDefs.back()) {
                    if (getDirective("include")) {
                        auto includeFile = getDirectiveValue();

                        if (!includeFile.has_value())
                            err::M0003.throwError("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");

                        if (includeFile->starts_with('"') && includeFile->ends_with('"'))
                            ; // Parsed path wrapped in ""
                        else if (includeFile->starts_with('<') && includeFile->ends_with('>'))
                            ; // Parsed path wrapped in <>
                        else
                            err::M0003.throwError("Expected path wrapped in \"path\" or <path>.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");

                        std::fs::path includePath = includeFile->substr(1, includeFile->length() - 2);

                        if (includePath.is_relative())
                            includePath = this->m_includeResolver(includePath);

                        if (!wolv::io::fs::isRegularFile(includePath)) {
                            if (includePath.parent_path().filename().string() == "std")
                                err::M0004.throwError("Path doesn't point to a valid file.", "This file might be part of the standard library. Make sure it's installed.");
                            else
                                err::M0004.throwError("Path doesn't point to a valid file.");
                        }

                        wolv::io::File file(includePath, wolv::io::File::Mode::Read);
                        if (!file.isValid()) {
                            err::M0005.throwError(fmt::format("Failed to open file.", *includeFile));
                        }

                        Preprocessor preprocessor(*this);
                        preprocessor.m_pragmas.clear();

                        auto result = preprocessor.preprocess(runtime, file.readString(), includePath.string(), false);

                        if (result.has_errs()) {
                            for (auto &item: result.errs) {
                                this->error(item);
                            }
                        }

                        bool shouldInclude = true;
                        if (preprocessor.shouldOnlyIncludeOnce()) {
                            auto [iter, added] = this->m_onceIncludedFiles.insert(includePath);
                            if (!added) {
                                shouldInclude = false;
                            }
                        }

                        std::copy(preprocessor.m_onceIncludedFiles.begin(), preprocessor.m_onceIncludedFiles.end(), std::inserter(this->m_onceIncludedFiles, this->m_onceIncludedFiles.begin()));
                        std::copy(preprocessor.m_defines.begin(), preprocessor.m_defines.end(), std::inserter(this->m_defines, this->m_defines.begin()));
                        std::copy(preprocessor.m_pragmas.begin(), preprocessor.m_pragmas.end(), std::inserter(this->m_pragmas, this->m_pragmas.begin()));

                        if (shouldInclude) {
                            auto content = result.unwrap();

                            std::replace(content.begin(), content.end(), '\n', ' ');
                            std::replace(content.begin(), content.end(), '\r', ' ');

                            content = wolv::util::trim(content);

                            if (!content.empty())
                                output += "/*! DOCS IGNORE ON **/ " + content + " /*! DOCS IGNORE OFF **/";
                        }
                    } else if (getDirective("define")) {
                        auto defineName = getDirectiveValue();
                        if (!defineName.has_value())
                            err::M0003.throwError("No name given in #define directive.", "A #define directive expects a name and a value in the form of #define NAME VALUE");

                        auto defineValue = getDirectiveValue(true);

                        this->m_defines[*defineName] = {defineValue.value_or(""), m_lineNumber };
                    } else if (getDirective("pragma")) {
                        auto pragmaKey = getDirectiveValue();
                        if (!pragmaKey.has_value())
                            err::M0003.throwError("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");

                        auto pragmaValue = getDirectiveValue(true);

                        this->m_pragmas[*pragmaKey].emplace_back(pragmaValue.value_or(""), m_lineNumber);
                    } else if (getDirective("error")) {
                        auto error = getDirectiveValue(true);

                        if (error.has_value())
                            err::M0007.throwError(error.value());
                        else
                            err::M0003.throwError("No value given to #error directive");
                    } else {
                        err::M0002.throwError("Expected 'include', 'define' or 'pragma'");
                    }
                }
            }

            if (m_offset >= code.length())
                break;

            if (code[m_offset] == '\n') {
                m_lineNumber++;
                m_lineBeginOffset = m_offset;
                startOfLine = true;
            } else if (!std::isspace(code[m_offset]))
                startOfLine = false;

            if (ifDefs.empty() || ifDefs.back() || code[m_offset] == '\n')
                output += code[m_offset];

            m_offset += 1;
        }

        // Apply defines
        std::vector<std::tuple<std::string, std::string, u32>> sortedDefines;
        std::for_each(this->m_defines.begin(), this->m_defines.end(), [&sortedDefines](const auto &entry){
            const auto &[key, data] = entry;
            const auto &[value, line] = data;

            sortedDefines.emplace_back(key, value, line);
        });
        std::sort(sortedDefines.begin(), sortedDefines.end(), [](const auto &left, const auto &right) {
            return std::get<0>(left).size() > std::get<0>(right).size();
        });

        for (const auto &[define, value, defineLine] : sortedDefines) {
            if (value.empty())
                continue;

            output = wolv::util::replaceStrings(output, define, value);
        }

        // Handle pragmas
        for (const auto &[type, datas] : this->m_pragmas) {
            for (const auto &data : datas) {
                const auto &[value, line] = data;

                if (this->m_pragmaHandlers.contains(type)) {
                    if (!this->m_pragmaHandlers[type](runtime, value))
                        err::M0006.throwError(fmt::format("Value '{}' cannot be used with the '{}' pragma directive.", value, type), { }, line);
                }
            }
        }

        this->m_defines.clear();

        return { output, m_errors };
    }

    void Preprocessor::addDefine(const std::string &name, const std::string &value) {
        this->m_defines[name] = { value, 0 };
    }

    void Preprocessor::addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler) {
        this->m_pragmaHandlers[pragmaType] = handler;
    }

    void Preprocessor::removePragmaHandler(const std::string &pragmaType) {
        this->m_pragmaHandlers.erase(pragmaType);
    }

    Location Preprocessor::location() {
        return { m_source, m_lineNumber, (u32) (m_offset - m_lineBeginOffset) };
    }

}