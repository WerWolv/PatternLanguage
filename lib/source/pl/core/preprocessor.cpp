#include <pl/core/preprocessor.hpp>

#include <wolv/utils/string.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/tokens.hpp>
#include <pl/core/parser.hpp>

namespace pl::core {

    Preprocessor::Preprocessor() : ErrorCollector() {
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });

        // register directive handlers
        registerDirectiveHandler(Token::Directive::IfDef, &Preprocessor::handleIfDef);
        registerDirectiveHandler(Token::Directive::IfNDef, &Preprocessor::handleIfNDef);
        registerDirectiveHandler(Token::Directive::Define, &Preprocessor::handleDefine);
        registerDirectiveHandler(Token::Directive::Undef, &Preprocessor::handleUnDefine);
        registerDirectiveHandler(Token::Directive::Pragma, &Preprocessor::handlePragma);
        registerDirectiveHandler(Token::Directive::Include, &Preprocessor::handleInclude);
        registerDirectiveHandler(Token::Directive::Error, &Preprocessor::handleError);
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
        this->m_keys = other.m_keys;
        this->m_initialized = false;
        this->m_source = other.m_source;
        this->m_output.clear();

        // need to update, because old handler points to old `this`
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    std::string getTokenValue(const Token &token) {
        auto identifier = token.getFormattedValue();
        if (identifier.length() > 1 )
            return identifier.substr(1, identifier.length() - 2);
        else
            return identifier;
    }

    std::string getOptionalValue(std::optional<Token> token) {
        if (!token.has_value())
            return "";
        return getTokenValue(token.value());
    }

    static bool isNameValid(const std::optional<pl::core::Token> &token) {
        if (!token.has_value())
            return false;
        auto name = getOptionalValue(token);
        if (name.empty())
            return false;
        // replace with std::ranges::all_of when available
        std::ranges::all_of(name, [](char c) {
            return std::isalnum(c) || c == '_';
        });

        return true;
    }

    bool operator==(const std::vector<pl::core::Token> &a, const std::vector<pl::core::Token> &b) {
        if (a.size() != b.size())
            return false;
        for (u32 i = 0; i < a.size(); i++)
            if (getTokenValue(a[i]) != getTokenValue(b[i]))
                return false;
        return true;
    }

    void removeValue(std::vector<pl::core::Token> &vec,const std::string &name) {
        for (u32 i = 0; i < vec.size(); i++)
            if (getTokenValue(vec[i]) == name)
                vec.erase(vec.begin() + i);
    }

    std::optional<pl::core::Token> Preprocessor::getDirectiveValue(u32 line) {
        const pl::core::Token &token = *m_token;
        if (m_token->location.line != line || m_token >= m_result.unwrap().end())
            return std::nullopt;
        m_token++;
        return token;
    }

    void Preprocessor::processIfDef(const bool add) {
        // find the next #endif
        const Location start = location();
        u32 depth = 1;
        while (m_token != m_result.unwrap().end() && depth > 0) {
            if (m_token->type == pl::core::Token::Type::Directive) {
                Token::Directive directive = get<Token::Directive>(m_token->value);
                if (directive == pl::core::Token::Directive::EndIf) {
                    m_token++;
                    depth--;
                }
            }
            if(add) process();
            else {
                if (m_token->type == pl::core::Token::Type::Directive) {
                    Token::Directive directive = get<Token::Directive>(m_token->value);
                    if (directive == pl::core::Token::Directive::IfDef ||
                        directive == pl::core::Token::Directive::IfNDef) {
                        depth++;
                        m_token++;
                    } else if (directive == pl::core::Token::Directive::EndIf) {
                        if (depth > 0) {
                            m_token++;
                            depth--;
                        }
                    }
                } else {
                    m_token++;
                }
            }
        }

        // if we didn't find an #endif, we have an error
        if(depth > 0) {
            errorAt(start, "#ifdef without #endif");
        }
    }

    void Preprocessor::handleIfDef(u32 line) {
        auto token = getDirectiveValue(line);

        if (!isNameValid(token)) {
            error("Expected identifier after #ifdef");
            return;
        }

        processIfDef(m_defines.contains( getOptionalValue(token)));
    }

    void Preprocessor::handleIfNDef(u32 line) {
        auto token = getDirectiveValue(line);

        if (!isNameValid(token)) {
            error("Expected identifier after #ifndef");
            return;
        }

        processIfDef(!m_defines.contains(getOptionalValue(token)));
    }

    void Preprocessor::handleDefine(u32 line) {
        auto token = getDirectiveValue(line);

        if (!isNameValid(token)) {
            error("Expected identifier after #define");
            return;
        }

        auto tokenValue = token.value();
        auto name = getTokenValue(tokenValue);

        std::vector<Token> values;
        auto replacement = getDirectiveValue(line);
        while (replacement.has_value()) {
            values.push_back(replacement.value());
            replacement = getDirectiveValue(line);
        }

        if (m_defines.contains(name)) {
            bool isValueNew = m_defines[name] == values;
            if (isValueNew) {
                errorAt(values[0].location, "Previous definition occurs at line '{}'.", m_defines[name][0].location.line);
                errorAt(m_defines[name][0].location, "Macro '{}' is redefined in line '{}'.", name, values[0].location.line);
                m_defines[name].clear();
                m_defines[name] = values;
                removeValue(m_keys, name);
                m_keys.emplace_back(tokenValue);
            }
        } else {
            m_defines[name] = values;
            m_keys.emplace_back(tokenValue);
        }
    }

    void Preprocessor::handleUnDefine(u32 line) {
        auto token = getDirectiveValue(line);

        if(!isNameValid(token)) {
            error("Expected identifier after #undef");
            return;
        }

        auto name = getOptionalValue(token);
        if (m_defines.contains(name)) {
            m_defines.erase(name);
            removeValue(m_keys, name);
        }
    }

    void Preprocessor::handlePragma(u32 line) {
        auto token = getDirectiveValue(line);

        if (!token.has_value()) {
            errorDesc("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");
            return;
        }

        auto key = getOptionalValue(token);
        token = getDirectiveValue(line);
        if (!token.has_value())
            this->m_pragmas[key].emplace_back("", line);
        else
            this->m_pragmas[key].emplace_back(getOptionalValue(token), line);
    }

    void Preprocessor::handleInclude(u32 line) {
        auto token = getDirectiveValue(line);
        if (!token.has_value()) {
            errorDesc("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        auto includeFile = getOptionalValue(token);

        if (includeFile.empty()){
            errorDesc("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        if (!((includeFile.starts_with('"') && includeFile.ends_with('"')) || (includeFile.starts_with('<') && includeFile.ends_with('>')))) {
            errorDesc("Invalid file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }

        const std::string includePath = includeFile.substr(1, includeFile.length() - 2);

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
        std::ranges::copy(preprocessor.m_keys.begin(), preprocessor.m_keys.end(), std::inserter(this->m_keys, this->m_keys.begin()));

        if (shouldInclude) {
            auto content = result.unwrap();

            if (!content.empty())
                for (auto entry : content) {
                    if (entry.type == pl::core::Token::Type::Separator) {
                        if (get<pl::core::Token::Separator>(entry.value) == pl::core::Token::Separator::EndOfProgram)
                            continue;
                    }
                    if (entry.type != pl::core::Token::Type::DocComment)
                        m_output.push_back(entry);
                }
        }
    }

    void Preprocessor::process() {
        u32 line = m_token->location.line;

        if (m_token->type == Token::Type::Directive) {
            Token::Directive directive = get<Token::Directive>(m_token->value);
            auto handler = m_directiveHandlers.find(directive);
            if (directive == Token::Directive::EndIf) {
                // Happens in nested #ifdefs
                return;
            } else if(handler == m_directiveHandlers.end()) {
                error("Unknown directive '{}'", m_token->getFormattedValue());
                m_token++;
                return;
            } else {
                m_token++;
                handler->second(this, line);
            }

        } else if (m_token->type == pl::core::Token::Type::Comment)
            m_token++;
        else {
            std::vector<pl::core::Token> values;
            std::vector<pl::core::Token> resultValues;
            values.push_back(*m_token);
            for (const auto &key: m_keys) {
                for (const auto &value: values) {

                    if (getTokenValue(value) == getTokenValue(key)) {
                        for (const auto &newToken: m_defines[getTokenValue(key)])
                            resultValues.push_back(newToken);
                    } else
                        resultValues.push_back(value);
                }
                values = resultValues;
                resultValues.clear();
            }

            for (const auto &value: values)
                m_output.push_back(value);
            m_token++;
        }
        return;
    }

    void Preprocessor::handleError(u32 line) {
        auto token = getDirectiveValue(line);

        if (token.has_value()) {
            auto message = token.value().getFormattedValue();
            error(message);
        } else {
            error("No message given in #error directive.");
        }
    }

    hlp::CompileResult<std::vector<Token>> Preprocessor::preprocess(PatternLanguage* runtime, api::Source* source, bool initialRun) {
        m_source = source;
        m_source->content = wolv::util::replaceStrings(m_source->content, "\r\n", "\n");
        m_source->content = wolv::util::replaceStrings(m_source->content, "\r", "\n");
        m_source->content = wolv::util::replaceStrings(m_source->content, "\t", "    ");

        m_runtime = runtime;
        m_output.clear();


        auto lexer = runtime->getInternals().lexer.get();

        if (initialRun) {
            this->m_onceIncludedFiles.clear();
            this->m_defines.clear();
            this->m_keys.clear();
            this->m_onlyIncludeOnce = false;
            this->m_pragmas.clear();

        }

        m_result = lexer->lex(m_source);
        if (m_result.hasErrs())
            return m_result;
        m_token = m_result.unwrap().begin();
        auto tokenEnd = m_result.unwrap().end();
        m_initialized = true;
        while (m_token < tokenEnd)
            process();

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

        return { m_output, collectErrors() };
    }

    void Preprocessor::addDefine(const std::string &name, const std::string &value) {
        m_defines[name] = { 
            pl::core::Token { 
                pl::core::Token::Type::Directive, 
                name, 
                location() 
            }, 
            pl::core::Token { 
                pl::core::Token::Type::String, 
                value, 
                location() 
            } 
        };
    }

    void Preprocessor::addPragmaHandler(const std::string &pragmaType, const api::PragmaHandler &handler) {
        this->m_pragmaHandlers[pragmaType] = handler;
    }

    void Preprocessor::addDirectiveHandler(const Token::Directive &directiveType, const api::DirectiveHandler &handler) {
        this->m_directiveHandlers[directiveType] = handler;
    }

    void Preprocessor::removePragmaHandler(const std::string &pragmaType) {
        this->m_pragmaHandlers.erase(pragmaType);
    }

    void Preprocessor::removeDirectiveHandler(const Token::Directive &directiveType) {
        this->m_directiveHandlers.erase(directiveType);
    }

    Location Preprocessor::location() {
        if (isInitialized())
            return m_token->location;
        else
            return { nullptr, 0, 0, 0 };
    }

    void Preprocessor::registerDirectiveHandler(const Token::Directive &name, auto memberFunction) {
        this->m_directiveHandlers[name] = [memberFunction](Preprocessor* preprocessor, u32 line){
            (preprocessor->*memberFunction)(line);
        };
    }

}