#include <pl/core/preprocessor.hpp>

#include <wolv/utils/string.hpp>

namespace pl::core {

    static bool isNameIdentifier(std::optional<std::string> name) {
        if (!name.has_value() || name->empty())
            return false;
        for (auto letter:*name)
            if (!std::isalnum(letter) && letter != '_')
                return false;
        return true;
    }

    Preprocessor::Preprocessor() : ErrorCollector() {
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });

        // register directive handlers
        registerDirectiveHandler("ifdef", &Preprocessor::handleIfDef);
        registerDirectiveHandler("ifndef", &Preprocessor::handleIfNDef);
        registerDirectiveHandler("define", &Preprocessor::handleDefine);
        registerDirectiveHandler("undef", &Preprocessor::handleUnDefine);
        registerDirectiveHandler("pragma", &Preprocessor::handlePragma);
        registerDirectiveHandler("include", &Preprocessor::handleInclude);
        registerDirectiveHandler("error", &Preprocessor::handleError);
    }

    Preprocessor::Preprocessor(const Preprocessor &other) : ErrorCollector(other) {
        this->m_defines = other.m_defines;
        this->m_replacements = other.m_replacements;
        this->m_unDefines = other.m_unDefines;
        this->m_docComments = other.m_docComments;
        this->m_pragmas = other.m_pragmas;
        this->m_onceIncludedFiles = other.m_onceIncludedFiles;
        this->m_resolver = other.m_resolver;
        this->m_onlyIncludeOnce = false;
        this->m_pragmaHandlers = other.m_pragmaHandlers;
        this->m_directiveHandlers = other.m_directiveHandlers;
        this->m_runtime = other.m_runtime;

        // need to update, because old handler points to old `this`
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    std::optional<std::string> Preprocessor::getDirectiveValue(const bool allowWhitespace) {
        while (std::isblank(peek())) {
            m_offset++;
            if (peek() == '\n' || peek() == '\r')
                return std::nullopt;
        }

        std::string value;
        char c = peek();
        while ((allowWhitespace || !std::isblank(c)) && c != '\n' && c != '\r') {
            value += c;

            c = m_code[++m_offset];
            if (m_offset >= m_code.length())
                break;
        }

        return wolv::util::trim(value);
    }

    std::string Preprocessor::parseDirectiveName() {
        const std::string_view view = &m_code[m_offset];
        auto end = view.find_first_of(" \t\n\r");

        if(end == std::string_view::npos)
            end = view.length();

        m_offset += end;

        return std::string(view.substr(0, end));
    }

    void Preprocessor::saveDocComment() {
        std::string comment = "";
        u32 lineCount = 0;
        if (peek() == '/' && peek(1) =='/' && peek(2) == '/') {
            comment += "///";
            m_offset += 3;
            while (peek() != 0 && peek() != '\n') {
                comment += peek();
                m_offset++;
            }
            m_offset++;
            lineCount = 1;
        } else {
            comment += "/*";
            this->m_offset += 2;
            comment += peek();
            this->m_offset++;
            while (peek() != 0 && (peek() != '*' || peek(1) != '/')) {
                if (peek() == '\n') {
                    this->m_lineNumber++;
                    this->m_lineBeginOffset = this->m_offset;
                    lineCount++;
                }
                comment += peek();
                this->m_offset++;
            }
            comment += "*/";
            this->m_offset += 2;
        }
        this->m_docComments.push_back(comment);
        this->m_output += (char) 0x80;
        this->m_output += this->m_docComments.size();
        this->m_output += std::string(lineCount, '\n');
    }

    // Avoid define replacements in doc comments by saving them to
    // a container and placing a marker and newlines to preserve line numbers
    void Preprocessor::parseComment() {
        char type = peek(1);
        char next = peek(2);

        if (type == '/') {
            if (next == '/') {
                saveDocComment();
                return;
            } else
                while (peek() != 0 && peek() != '\n') m_offset++;

        } else if (type == '*') {

            if ((next == '*' && peek(3) != '/') || next == '!') {
                saveDocComment();
                return;
            }
            while (peek() != 0 && (peek() != '*' || peek(1) != '/')) {
                // update lines
                if (peek() == '\n') {
                    m_output += '\n';
                    m_lineNumber++;
                    m_lineBeginOffset = m_offset;
                }

                m_offset++;
            }

            m_offset += 2;
            // add spaces to ensure column location of errors is correct.
            u32 i = 0;
            bool needsSpaces = false;
            const std::string_view view = &m_code[m_offset];
            auto end = view.find_first_of("\0\n\r");

            if(end == std::string_view::npos)
                end = view.length();

            while (i < end) {
                if (!std::isspace(view[i])) {
                    needsSpaces = true;
                    break;
                }
                i++;
            }
            if (needsSpaces) {
                u32 spaceCount = m_offset - m_lineBeginOffset;
                m_output += std::string(spaceCount, ' ');
            }
        }
    }

    void Preprocessor::processIfDef(const bool add) {
        // find the next #endif
        const Location start = location();
        u32 depth = 1;
        while(peek() != 0 && depth > 0) {
            if(m_code.substr(m_offset, 6) == "#endif" && !m_inString && m_startOfLine) {
                m_offset += 6;
                depth--;
            }
            if(add) process();
            else {
                char c = peek();
                if(c == '\n') {
                    m_lineNumber++;
                    m_lineBeginOffset = m_offset;
                    m_startOfLine = true;
                    m_output += c;
                } else if (c == '#' && !m_inString && m_startOfLine) {
                    m_offset++;
                    // handle ifdef and ifndef
                    std::string name = parseDirectiveName();
                    if(name == "ifdef" || name == "ifndef") {
                        depth++;
                    }
                } else if(!std::isspace(c)) {
                    m_startOfLine = false;
                } else if(c == '"') {
                    m_inString = !m_inString;
                }

                m_offset++;
            }
        }

        // if we didn't find an #endif, we have an error
        if(depth > 0) {
            errorAt(start, "#ifdef without #endif");
        }
    }

    void Preprocessor::handleIfDef() {
        auto name = getDirectiveValue();

        if(!name.has_value() || !isNameIdentifier(name)) {
            error("Expected identifier after #ifdef");
            return;
        }

        processIfDef(m_defines.contains(name.value()));
    }

    void Preprocessor::handleIfNDef() {
        auto name = getDirectiveValue();

        if(!name.has_value() || !isNameIdentifier(name)) {
            error("Expected identifier after #ifndef");
            return;
        }

        processIfDef(!m_defines.contains(name.value()));
    }

    void Preprocessor::handleDefine() {
        auto name = getDirectiveValue();

        if(!name.has_value() || !isNameIdentifier(name)) {
            error("Expected identifier after #define");
            return;
        }

        auto value = getDirectiveValue(true);

        if(!value.has_value()) {
            error("Expected value after #define");
            return;
        }
        if (m_unDefines.contains(name.value()))
            m_unDefines.erase(name.value());
        else if (m_defines.contains(name.value()) && value.value() != m_defines[name.value()].first) {
            m_replacements[m_defines[name.value()].second] =
                    { name.value(), m_defines[name.value()].first,m_lineNumber };
            ::fmt::print("[WARN] : Macro {} was redefined in line {} from its previous value in line and {} \n", name.value(), m_lineNumber, m_defines[name.value()].second);
        }

        m_defines[name.value()] = { value.value(), m_lineNumber };
    }

    void Preprocessor::handleUnDefine() {
        auto name = getDirectiveValue();

        if(!name.has_value() || !isNameIdentifier(name)) {
            error("Expected identifier after #undef");
            return;
        }

        if (m_defines.contains(name.value())) {
            m_replacements[m_defines[name.value()].second] = { name.value(), m_defines[name.value()].first, m_lineNumber };
            m_defines.erase(name.value());
        }

        m_unDefines[name.value()] = m_lineNumber;
    }

    void Preprocessor::handlePragma() {
        auto key = getDirectiveValue();

        if(!key.has_value()) {
            errorDesc("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");
            return;
        }

        auto value = getDirectiveValue(true);

        this->m_pragmas[key.value()].emplace_back(value.value_or(""), m_lineNumber);
    }

    void Preprocessor::handleInclude() {
        const auto includeFile = getDirectiveValue();

        if (!includeFile.has_value()) {
            errorDesc("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        if (includeFile->starts_with('"') && includeFile->ends_with('"'))
            ; // Parsed path wrapped in ""
        else if (includeFile->starts_with('<') && includeFile->ends_with('>'))
            ; // Parsed path wrapped in <>
        else {
            errorDesc("Invalid file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        const std::string includePath = includeFile->substr(1, includeFile->length() - 2);

        // determine if we should include this file
        if (this->m_onceIncludedFiles.contains(includePath))
            return;

        if(!m_resolver) {
            errorDesc("Unable to lookup results", "No include resolver was set.");
            return;
        }

        auto [resolved, error] = this->m_resolver(includePath);

        if(!resolved.has_value()) {
            for (const auto &item: error) {
                this->error(item);
            }
            return;
        }

        Preprocessor preprocessor(*this);
        preprocessor.m_pragmas.clear();

        auto result = preprocessor.preprocess(m_runtime, resolved.value(), false);

        if (result.hasErrs()) {
            for (auto &item: result.errs) {
                this->error(item);
            }
            return;
        }

        resolved.value()->content = result.unwrap(); // cache result

        bool shouldInclude = true;
        if (preprocessor.shouldOnlyIncludeOnce()) {
            auto [iter, added] = this->m_onceIncludedFiles.insert(includePath);
            if (!added) {
                shouldInclude = false;
            }
        }

        std::ranges::copy(preprocessor.m_onceIncludedFiles.begin(), preprocessor.m_onceIncludedFiles.end(), std::inserter(this->m_onceIncludedFiles, this->m_onceIncludedFiles.begin()));
        std::ranges::copy(preprocessor.m_defines.begin(), preprocessor.m_defines.end(), std::inserter(this->m_defines, this->m_defines.begin()));
        std::ranges::copy(preprocessor.m_pragmas.begin(), preprocessor.m_pragmas.end(), std::inserter(this->m_pragmas, this->m_pragmas.begin()));

        if (shouldInclude) {
            auto content = result.unwrap();

            std::ranges::replace(content.begin(), content.end(), '\n', ' ');
            std::ranges::replace(content.begin(), content.end(), '\r', ' ');

            content = wolv::util::trim(content);

            if (!content.empty())
                m_output += "/*! DOCS IGNORE ON **/ " + content + " /*! DOCS IGNORE OFF **/";
        }
    }

    bool Preprocessor::process() {
        char c = peek();

        if(c == '\0')
            return false;

        if (c == '\\') {
            if (m_inString) {
                m_output += m_code.substr(m_offset, 2);
                m_offset += 2;
                return true;
            } else
                return false;
        }

        if(c == '"') {
            m_inString = !m_inString;
        } else if (m_inString) {
            m_output += peek();
            m_offset += 1;
            return true;
        }

        if(c == '/') { // comment case
            parseComment();
        }

        if (peek() == '#' && m_startOfLine && !m_inString) {
            m_offset += 1;

            std::string name = parseDirectiveName();

            auto handler = m_directiveHandlers.find(name);

            if(handler == m_directiveHandlers.end()) {
                error("Unknown directive '{}'", name);
                return true;
            } else {
                handler->second(this);
                if (peek() == '#')
                    return true;
            }
        }

        if (m_offset >= m_code.length())
            return false;

        c = peek();

        if (c == '\n') {
            m_lineNumber++;
            m_lineBeginOffset = m_offset;
            m_startOfLine = true;
        }

        if (!std::isspace(c))
            m_startOfLine = false;


        m_output += c;
        m_offset += 1;

        return true;
    }

    void Preprocessor::handleError() {
        auto message = getDirectiveValue(true);

        if(message.has_value()) {
            error(message.value());
        } else {
            error("No message given in #error directive.");
        }
    }

    void Preprocessor::replaceSkippingStrings(std::string &input, const std::string &find, const std::string &replace) {
        auto parts = wolv::util::splitString(input, "\"");

        for (u32 i = 0; i < parts.size(); i += 2) {
            while (1 < i && i < parts.size() && !parts[i - 1].empty() && parts[i - 1].ends_with('\\')) {
                parts[i - 1] = wolv::util::combineStrings({parts[i - 1], parts[i]}, "\"");
                parts.erase(parts.begin() + i);
            }
            parts[i] = wolv::util::replaceStrings(parts[i], find, replace);
        }
        input = wolv::util::combineStrings(parts, "\"");
    }

    hlp::CompileResult<std::string> Preprocessor::preprocess(PatternLanguage* runtime, api::Source* source, bool initialRun) {
        m_startOfLine = true;
        m_offset      = 0;
        m_lineNumber  = 1;
        m_code        = source->content;
        m_source      = source;
        m_inString    = false;
        m_runtime     = runtime;
        m_lineBeginOffset = 0;
        m_output.clear();

        if (initialRun) {
            this->m_onceIncludedFiles.clear();
            this->m_onlyIncludeOnce = false;
            this->m_pragmas.clear();
        }

        m_output.reserve(m_code.size());

        while (m_offset < m_code.length()) {
            if (!process())
                break;
        }
        // defines from included files apply to entire file and can be identified
        // from a line number of zero.
        std::vector<std::pair<std::string, std::string >> includedDefines;
        for (auto define:m_defines) {
            if (define.second.second != 0)
                m_replacements[define.second.second] = {define.first, define.second.first, m_lineNumber };
            else
                includedDefines.emplace_back(define.first, define.second.first);
        }

        // Apply defines
        auto lines = wolv::util::splitString(m_output, "\n");
        // first defines from included files. order is not important.
        for (auto &[name, value]: includedDefines) {
            if (m_output.find(name) != std::string::npos)
                for (auto line = lines.begin(); line != lines.end(); line++)
                    replaceSkippingStrings(*line, name, value);
        }


        for (auto &[start, data]: this->m_replacements) {
            auto [name, value, end] = data;
            u32 i = 0;
            end = std::min(end, (u32) lines.size());
            for ( i = start; i < end; i++) {
                auto &line = lines[i];
                if (!line.empty() && line.find(name) != std::string::npos)
                    replaceSkippingStrings(line, name, value);
            }
        }

        m_output = wolv::util::combineStrings(lines, "\n");


        // restore doc comments
        unsigned long long pos = 0;
        while ((pos = m_output.find((char) 0x80, pos)) != std::string::npos) {
            if ( pos + 1 < m_output.length()) {
                unsigned commentIndex = m_output[pos + 1]-1;
                if (commentIndex < m_docComments.size()) {
                    auto comment = m_docComments[commentIndex];
                    auto linesInComment = std::count(comment.begin(), comment.end(), '\n');
                    m_output.replace(pos, 2 + linesInComment, m_docComments[commentIndex]);
                    pos += m_docComments[commentIndex].length();
                } else
                    break;
            } else
                break;
        }

        // Handle pragmas
        for (const auto &[type, datas] : this->m_pragmas) {
            for (const auto &data : datas) {
                const auto &[value, line] = data;

                if (this->m_pragmaHandlers.contains(type)) {
                    if (!this->m_pragmaHandlers[type](*runtime, value))
                        errorAt(Location { m_source, line, 1, value.length() },
                                 "Value '{}' cannot be used with the '{}' pragma directive.", value, type);
                }
            }
        }

        this->m_defines.clear();
        this->m_unDefines.clear();
        this->m_replacements.clear();

        return { m_output, collectErrors() };
    }

    void Preprocessor::addDefine(const std::string &name, const std::string &value) {
        this->m_defines[name] = { value, 0 };
    }

    void Preprocessor::addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler) {
        this->m_pragmaHandlers[pragmaType] = handler;
    }

    void Preprocessor::addDirectiveHandler(const std::string &directiveType, const api::DirectiveHandler &handler) {
        this->m_directiveHandlers[directiveType] = handler;
    }

    void Preprocessor::removePragmaHandler(const std::string &pragmaType) {
        this->m_pragmaHandlers.erase(pragmaType);
    }

    void Preprocessor::removeDirectiveHandler(const std::string &directiveType) {
        this->m_directiveHandlers.erase(directiveType);
    }

    Location Preprocessor::location() {
        return { m_source, m_lineNumber, (u32) (m_offset - m_lineBeginOffset), 1 };
    }

    void Preprocessor::registerDirectiveHandler(const std::string& name, auto memberFunction) {
        this->m_directiveHandlers[name] = [memberFunction](Preprocessor* preprocessor) {
            (preprocessor->*memberFunction)();
        };
    }

}