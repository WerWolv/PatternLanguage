#include <pl/core/preprocessor.hpp>

#include <wolv/utils/string.hpp>

namespace pl::core {

    Preprocessor::Preprocessor() : ErrorCollector() {
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });

        // register directive handlers
        registerDirectiveHandler("ifdef", &Preprocessor::handleIfDef);
        registerDirectiveHandler("ifndef", &Preprocessor::handleIfNDef);
        registerDirectiveHandler("define", &Preprocessor::handleDefine);
        registerDirectiveHandler("pragma", &Preprocessor::handlePragma);
        registerDirectiveHandler("include", &Preprocessor::handleInclude);
        registerDirectiveHandler("error", &Preprocessor::handleError);
    }

    Preprocessor::Preprocessor(const Preprocessor &other) : ErrorCollector(other) {
        this->m_defines = other.m_defines;
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

    void Preprocessor::parseComment() {
        char type = peek(1);
        if(type == '/') {
            while(peek() != 0 && peek() != '\n') m_offset++;
        } else if(type == '*') {
            char next = peek(2);
            if(next == '*' || next == '!')
                return;
            while(peek() != 0 && (peek() != '*' || peek(1) != '/')) {
                // update lines
                if(peek() == '\n') {
                    m_lineNumber++;
                    m_lineBeginOffset = m_offset;
                }

                m_offset++;
            }

            m_offset += 2;
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
            error_at(start, "#ifdef without #endif");
        }
    }

    void Preprocessor::handleIfDef() {
        auto name = getDirectiveValue();

        if(!name.has_value()) {
            error("Expected identifier after #ifdef");
            return;
        }

        processIfDef(m_defines.contains(name.value()));
    }

    void Preprocessor::handleIfNDef() {
        auto name = getDirectiveValue();

        if(!name.has_value()) {
            error("Expected identifier after #ifndef");
            return;
        }

        processIfDef(!m_defines.contains(name.value()));
    }

    void Preprocessor::handleDefine() {
        auto name = getDirectiveValue();

        if(!name.has_value()) {
            error("Expected identifier after #define");
            return;
        }

        auto value = getDirectiveValue(true);

        if(!value.has_value()) {
            error("Expected value after #define");
            return;
        }

        m_defines[name.value()] = { value.value(), m_lineNumber };
    }

    void Preprocessor::handlePragma() {
        auto key = getDirectiveValue();

        if(!key.has_value()) {
            error_desc("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");
            return;
        }

        auto value = getDirectiveValue(true);

        this->m_pragmas[key.value()].emplace_back(value.value_or(""), m_lineNumber);
    }

    void Preprocessor::handleInclude() {
        const auto includeFile = getDirectiveValue();

        if (!includeFile.has_value()) {
            error_desc("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        if (includeFile->starts_with('"') && includeFile->ends_with('"'))
            ; // Parsed path wrapped in ""
        else if (includeFile->starts_with('<') && includeFile->ends_with('>'))
            ; // Parsed path wrapped in <>
        else {
            error_desc("Invalid file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        const std::string includePath = includeFile->substr(1, includeFile->length() - 2);

        // determine if we should include this file
        if (this->m_onceIncludedFiles.contains(includePath))
            return;

        if(m_resolver == nullptr) {
            error_desc("Unable to lookup results", "No include resolver was set.");
            return;
        }

        auto [resolved, error] = this->m_resolver->resolve(includePath);

        if(!resolved.has_value()) {
            for (const auto &item: error) {
                this->error(item);
            }
            return;
        }

        Preprocessor preprocessor(*this);
        preprocessor.m_pragmas.clear();

        auto result = preprocessor.preprocess(m_runtime, resolved.value(), false);

        if (result.has_errs()) {
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

        if(c == '"' and peek(-1) != '\\') {
            m_inString = !m_inString;
        }

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

    CompileResult<std::string> Preprocessor::preprocess(PatternLanguage* runtime, api::Source* source, bool initialRun) {
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

        // Apply defines
        std::vector<std::tuple<std::string, std::string, u32>> sortedDefines;
        std::ranges::for_each(this->m_defines.begin(), this->m_defines.end(), [&sortedDefines](const auto &entry){
            const auto &[key, data] = entry;
            const auto &[value, line] = data;

            sortedDefines.emplace_back(key, value, line);
        });
        std::ranges::sort(sortedDefines.begin(), sortedDefines.end(), [](const auto &left, const auto &right) {
            return std::get<0>(left).size() > std::get<0>(right).size();
        });

        for (const auto &[define, value, defineLine] : sortedDefines) {
            if (value.empty())
                continue;

            m_output = wolv::util::replaceStrings(m_output, define, value);
        }

        // Handle pragmas
        auto& runtimeRef = *runtime;

        for (const auto &[type, datas] : this->m_pragmas) {
            for (const auto &data : datas) {
                const auto &[value, line] = data;

                if (this->m_pragmaHandlers.contains(type)) {
                    if (!this->m_pragmaHandlers[type](runtimeRef, value))
                        error_at(Location { m_source, line, 1, value.length() },
                                 "Value '{}' cannot be used with the '{}' pragma directive.", value, type);
                }
            }
        }

        this->m_defines.clear();

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