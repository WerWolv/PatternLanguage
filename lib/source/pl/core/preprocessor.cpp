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
        this->m_keys = other.m_keys;
        this->m_initialized = false;

        // need to update, because old handler points to old `this`
        this->addPragmaHandler("once", [this](PatternLanguage&, const std::string &value) {
            this->m_onlyIncludeOnce = true;

            return value.empty();
        });
    }

    static bool isValidIdentifier(const std::optional<Token> &token) {
        if (!token.has_value())
            return false;
        const Token::Identifier *identifier = std::get_if<Token::Identifier>(&token->value);
        auto name = identifier->get();
        if (name.empty())
            return false;
        return std::ranges::all_of(name, [](char c) {
            return std::isalnum(c) || c == '_';
        });

        return true;
    }

    bool operator==(const std::vector<Token>& a, const std::vector<Token>& b) {
        if (a.size() != b.size())
            return false;
        for (u32 i = 0; i < a.size(); i++)
            if (a[i].type != b[i].type || a[i].value != b[i].value || a[i].location != b[i].location)
                return false;
        return true;
    }

    void Preprocessor::removeKey(const Token &token) {
        for (u32 i = 0; i < m_keys.size(); i++)
            if (m_keys[i].value == token.value)
                m_keys.erase(m_keys.begin() + i);
    }

    void Preprocessor::processIfDef(const bool add) {
        // find the next #endif
        const Location start = location();
        if (!add) {
            Location startExclude = start;
            startExclude.column = 0;
            m_excludedLocations.push_back({true, startExclude});
        }
        u32 depth = 1;
        while(!eof() && depth > 0) {
            if (auto *directive = std::get_if<Token::Directive>(&m_token->value); directive != nullptr && *directive == Token::Directive::EndIf) {
                depth--;
                if (depth == 0 && !add) {
                    auto location = m_token->location;
                    location.column = 0;
                    m_excludedLocations.push_back({false, location});
                }
                m_token++;
                continue;
            }
            if(add) {
                process();
            } else {
                if (auto *directive = std::get_if<Token::Directive>(&m_token->value);directive != nullptr &&
                    (*directive == Token::Directive::IfDef || *directive == Token::Directive::IfNDef))
                    depth++;
                m_token++;
            }
        }

        // if we didn't find an #endif, we have an error
        if(depth > 0) {
            errorAt(start, "#ifdef without #endif");
        }
    }

    void Preprocessor::handleIfDef(u32 line) {

        auto *identifier = std::get_if<Token::Identifier>(&m_token->value);
        auto token = *m_token;
        if (m_token->location.line != line || identifier == nullptr || !isValidIdentifier(token)) {
            error("Expected identifier after #ifdef");
            return;
        } else
            identifier->setType(Token::Identifier::IdentifierType::Macro);
        m_token++;
        processIfDef(m_defines.contains(identifier->get()));
    }

    void Preprocessor::handleIfNDef(u32 line) {

        auto *identifier = std::get_if<Token::Identifier>(&m_token->value);
        auto token = *m_token;
        if (m_token->location.line != line || identifier == nullptr || !isValidIdentifier(token)) {
            error("Expected identifier after #ifdef");
            return;
        } else
            identifier->setType(Token::Identifier::IdentifierType::Macro);
        m_token++;
        processIfDef(!m_defines.contains(identifier->get()));
    }

    void Preprocessor::handleDefine(u32 line) {

        auto *tokenIdentifier = std::get_if<Token::Identifier>(&m_token->value);
        std::string name;
        auto token = *m_token;
        if (m_token->location.line != line || tokenIdentifier == nullptr || !isValidIdentifier(token)) {
            error("Expected identifier after #define");
            return;
        } else {
            tokenIdentifier->setType(Token::Identifier::IdentifierType::Macro);
            name = tokenIdentifier->get();
        }
        m_token++;

        std::vector<Token> values;
        while (m_token->location.line == line) {
            values.push_back(*m_token);
            m_token++;
        }

        if (m_defines.contains(name)) {
            bool isValueSame = m_defines[name] == values;
            if (!isValueSame) {
                errorAt(values[0].location, "Previous definition occurs at line '{}'.", m_defines[name][0].location.line);
                errorAt(m_defines[name][0].location, "Macro '{}' is redefined in line '{}'.", name, values[0].location.line);
                m_defines[name].clear();
                m_defines[name] = values;
                removeKey(token);
                m_keys.emplace_back(token);
            }
        } else {
            m_defines[name] = values;
            m_keys.emplace_back(token);
        }
    }

    void Preprocessor::handleUnDefine(u32 line) {

        auto *tokenIdentifier = std::get_if<Token::Identifier>(&m_token->value);
        auto token = *m_token;
        std::string name;
        if (m_token->location.line != line || tokenIdentifier == nullptr || !isValidIdentifier(token)) {
            error("Expected identifier after #ifdef");
            return;
        } else {
            tokenIdentifier->setType(Token::Identifier::IdentifierType::Macro);
            name = tokenIdentifier->get();
        }
        m_token++;
        if (m_defines.contains(name)) {
            m_defines.erase(name);
            removeKey(token);
        }
    }

    void Preprocessor::handlePragma(u32 line) {
        auto *tokenLiteral = std::get_if<Token::Literal>(&m_token->value);
        if (tokenLiteral == nullptr) {
            errorDesc("No instruction given in #pragma directive.", "A #pragma directive expects a instruction followed by an optional value in the form of #pragma <instruction> <value>.");
            return;
        }
        m_token++;
        auto key = tokenLiteral->toString(false);
        tokenLiteral = std::get_if<Token::Literal>(&m_token->value);
        if (m_token->location.line != line || tokenLiteral == nullptr)
            this->m_pragmas[key].emplace_back("", line);
        else {
            std::string value = tokenLiteral->toString(false);
            this->m_pragmas[key].emplace_back(value, line);
            m_token++;
        }
    }

    void Preprocessor::handleInclude(u32 line) {
        auto *tokenLiteral = std::get_if<Token::Literal>(&m_token->value);
        if (tokenLiteral == nullptr || m_token->location.line != line) {
            errorDesc("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }
        auto includeFile = tokenLiteral->toString(false);

        if (!(includeFile.starts_with('"') && includeFile.ends_with('"')) && !(includeFile.starts_with('<') && includeFile.ends_with('>'))) {
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
        m_token++;
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
                    if (auto *separator = std::get_if<Token::Separator>(&entry.value); separator != nullptr && *separator == Token::Separator::EndOfProgram)
                        continue;
                    if (entry.type != Token::Type::DocComment)
                        m_output.push_back(entry);
                }
        }
    }

    void Preprocessor::process() {
        u32 line = m_token->location.line;

        if (auto *directive = std::get_if<Token::Directive>(&m_token->value); directive != nullptr ) {
            auto handler = m_directiveHandlers.find(*directive);
            if(handler == m_directiveHandlers.end()) {
                error("Unknown directive '{}'", m_token->getFormattedValue());
                m_token++;
                return;
            } else {
                m_token++;
                handler->second(this, line);
            }

        } else if (m_token->type == Token::Type::Comment)
            m_token++;
        else {
            std::vector<Token> values;
            std::vector<Token> resultValues;
            values.push_back(*m_token);
            for (const auto &key: m_keys) {
                const Token::Identifier *keyIdentifier = std::get_if<Token::Identifier>(&key.value);
                for (const auto &value: values) {
                    const Token::Identifier *valueIdentifier = std::get_if<Token::Identifier>(&value.value);
                    if (valueIdentifier == nullptr)
                        resultValues.push_back(value);
                   else if (valueIdentifier->get() == keyIdentifier->get() ) {
                        Token::Identifier *tokenIdentifier = std::get_if<Token::Identifier>(&m_token->value);
                        if (tokenIdentifier != nullptr)
                            tokenIdentifier->setType(Token::Identifier::IdentifierType::Macro);
                        for (const auto &newToken: m_defines[keyIdentifier->get()])
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
    }

    void Preprocessor::handleError(u32 line) {
        auto *tokenLiteral = std::get_if<Token::Literal>(&m_token->value);

        auto token =*m_token;

        if(tokenLiteral != nullptr && m_token->location.line == line) {
            auto message = tokenLiteral->toString(false);
            m_token++;
            tokenLiteral = std::get_if<Token::Literal>(&m_token->value);
            if (tokenLiteral != nullptr && m_token->location.line == line) {
                message += " " + tokenLiteral->toString(false);
                m_token++;
            }
            error(message);
        } else {
            error("No message given in #error directive.");
        }
    }

    void Preprocessor::appendExcludedLocation(const ExcludedLocation &location) {

        auto it = std::find_if(m_excludedLocations.begin(), m_excludedLocations.end(), [&location](const ExcludedLocation& el) {
            return el.isExcluded == location.isExcluded;
        });

        if (it == m_excludedLocations.end()) {
            m_excludedLocations.push_back(location);
        }
    }

    void Preprocessor::validateExcludedLocations() {
        auto size = m_excludedLocations.size();
        if (size == 0)
            return;
        auto excludedLocations = m_excludedLocations;
        m_excludedLocations.clear();
        for (auto &location : excludedLocations)
            appendExcludedLocation(location);
    }

    void Preprocessor::validateOutput() {
        std::vector<Token> output = m_output;
        m_output.clear();
        for (const auto &token : output) {
            if (token.type == Token::Type::Comment || token.type == Token::Type::Directive)
                continue;
            m_output.push_back(token);
        }
    }

    hlp::CompileResult<std::vector<Token>> Preprocessor::preprocess(PatternLanguage* runtime, api::Source* source, bool initialRun) {
        m_source = source;
        m_source->content = wolv::util::replaceStrings(m_source->content, "\r\n", "\n");
        m_source->content = wolv::util::replaceStrings(m_source->content, "\r", "\n");
        m_source->content = wolv::util::replaceStrings(m_source->content, "\t", "    ");
        while (m_source->content.ends_with('\n'))
            m_source->content.pop_back();

        m_runtime = runtime;
        m_output.clear();

        auto lexer = runtime->getInternals().lexer.get();

        if (initialRun) {
            this->m_excludedLocations.clear();
            this->m_onceIncludedFiles.clear();
            this->m_defines.clear();
            this->m_keys.clear();
            this->m_onlyIncludeOnce = false;
            this->m_pragmas.clear();
            for (const auto& [name, value] : m_runtime->getDefines()) {
                addDefine(name, value);
            }
            for (const auto& [name, handler]: m_runtime->getPragmas()) {
                addPragmaHandler(name, handler);
            }
        }

        auto [result,errors] = lexer->lex(m_source);
        if (result.has_value())
            m_result = std::move(result.value());
        else
            return { std::nullopt, errors };
        if (!errors.empty())
            for (auto &item: errors)
                this->error(item);

        m_token = m_result.begin();
        m_initialized = true;
        while (!eof())
            process();

        validateExcludedLocations();
        // Handle pragmas
        for (const auto &[type, datas] : this->m_pragmas) {
            for (const auto &data : datas) {
                const auto &[value, line] = data;

                if (this->m_pragmaHandlers.contains(type)) {
                    if (!this->m_pragmaHandlers[type](*m_runtime, value))
                        errorAt(Location { m_source, line, 1, value.length() },
                                "Value '{}' cannot be used with the '{}' pragma directive.", value, type);
                }
            }
        }
        validateOutput();

        m_initialized = false;

        return { m_output, collectErrors() };
    }

    bool Preprocessor::eof() {
        return m_token == m_result.end();
    }

    void Preprocessor::addDefine(const std::string &name, const std::string &value) {
        m_defines[name] = {Token { Token::Type::String, value, {nullptr, 0, 0, 0 } } } ;
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
            return {nullptr, 0, 0, 0 };
    }

    void Preprocessor::registerDirectiveHandler(const Token::Directive& name, auto memberFunction) {
        this->m_directiveHandlers[name] = [memberFunction](Preprocessor* preprocessor, u32 line){
            (preprocessor->*memberFunction)(line);
        };
    }

}