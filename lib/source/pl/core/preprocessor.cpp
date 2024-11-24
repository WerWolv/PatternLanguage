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
        registerStatementHandler(Token::Keyword::Import, &Preprocessor::handleImport);
    }

    Preprocessor::Preprocessor(const Preprocessor &other) : ErrorCollector(other) {
        this->m_defines = other.m_defines;
        this->m_pragmas = other.m_pragmas;
        this->m_onceIncludedFiles = other.m_onceIncludedFiles;
        this->m_resolver = other.m_resolver;
        this->m_onlyIncludeOnce = false;
        this->m_pragmaHandlers = other.m_pragmaHandlers;
        this->m_directiveHandlers = other.m_directiveHandlers;
        this->m_statementHandlers = other.m_statementHandlers;
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
    }

    bool operator==(const std::vector<Token>& a, const std::vector<Token>& b) {
        if (a.size() != b.size())
            return false;

        for (u32 i = 0; i < a.size(); i += 1) {
            if (a[i].type != b[i].type || a[i].value != b[i].value || a[i].location != b[i].location)
                return false;
        }
        return true;
    }

    void Preprocessor::nextLine(u32 line) {
        while (!eof() && m_token->location.line == line) {
            if (auto *separator = std::get_if<Token::Separator>(&m_token->value);
                    (separator != nullptr && *separator == Token::Separator::EndOfProgram) ||
                    m_token->type == Token::Type::Comment || m_token->type == Token::Type::DocComment)
                m_output.push_back(*m_token);
            m_token++;
        }
    }

    void Preprocessor::removeKey(const Token &token) {
        for (u32 i = 0; i < m_keys.size(); i++) {
            if (m_keys[i].value == token.value) {
                m_keys.erase(m_keys.begin() + i);
            }
        }
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
                nextLine(m_token->location.line);
                continue;
            }
            if(add) {
                process();
            } else {
                if (auto *directive = std::get_if<Token::Directive>(&m_token->value);directive != nullptr &&
                    (*directive == Token::Directive::IfDef || *directive == Token::Directive::IfNDef))
                    depth++;
                nextLine(m_token->location.line);
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
        nextLine(line);
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
        nextLine(line);
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
            if (eof()){
                values.pop_back();
                m_token--;
                break;
            }
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
        nextLine(line);
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
        // get include name
        auto *tokenLiteral = std::get_if<Token::Literal>(&m_token->value);
        std::string path;
        if (tokenLiteral != nullptr && m_token->type == Token::Type::String) {
            path = tokenLiteral->toString(false);
            if (path.contains('.'))
                path = wolv::util::replaceStrings(path, ".", "/");
        } else if (tokenLiteral == nullptr || m_token->location.line != line) {
            errorDesc("No file to include given in #include directive.", "A #include directive expects a path to a file: #include \"path/to/file\" or #include <path/to/file>.");
            return;
        }
        m_token++;

        if(!m_resolver) {
            errorDesc("Unable to lookup results", "No include resolver was set.");
            return;
        }

        auto [resolved, error] = this->m_resolver(path);

        if(!resolved.has_value()) {
            for (const auto &item: error) {
                this->error(item);
            }
            return;
        }
        // determine if we should include this file
        if (this->m_onceIncludedFiles.contains({resolved.value(),""}))
            return;

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
            auto [iter, added] = this->m_onceIncludedFiles.insert({resolved.value(), ""});
            if (!added) {
                shouldInclude = false;
            }
        }

        std::ranges::copy(preprocessor.m_onceIncludedFiles.begin(), preprocessor.m_onceIncludedFiles.end(), std::inserter(this->m_onceIncludedFiles, this->m_onceIncludedFiles.begin()));
        std::ranges::copy(preprocessor.m_defines.begin(), preprocessor.m_defines.end(), std::inserter(this->m_defines, this->m_defines.begin()));
        std::ranges::copy(preprocessor.m_pragmas.begin(), preprocessor.m_pragmas.end(), std::inserter(this->m_pragmas, this->m_pragmas.begin()));
        std::ranges::copy(preprocessor.m_keys.begin(), preprocessor.m_keys.end(), std::inserter(this->m_keys, this->m_keys.begin()));
        std::ranges::copy(preprocessor.m_namespaces.begin(), preprocessor.m_namespaces.end(), std::inserter(this->m_namespaces, this->m_namespaces.begin()));

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
        nextLine(line);
    }

    void Preprocessor::handleImport(u32 line) {
        std::vector<Token> saveImport;
        saveImport.push_back(m_token[-1]);
        saveImport.push_back(*m_token);
        // get include name
        auto *tokenLiteral = std::get_if<Token::Literal>(&m_token->value);
        std::string path;
        if (m_token->type == Token::Type::String) {
            path = tokenLiteral->toString(false);
        } else if (m_token->type == Token::Type::Identifier) {
            path = std::get_if<Token::Identifier>(&m_token->value)->get();
            m_token++;
            auto *separator = std::get_if<Token::Separator>(&m_token->value);
            while (separator != nullptr && *separator == Token::Separator::Dot) {
                saveImport.push_back(*m_token);
                m_token++;
                if (m_token->type != Token::Type::Identifier) {
                    error("Expected identifier after '.' in import statement.");
                    return;
                }
                path += "/" + std::get_if<Token::Identifier>(&m_token->value)->get();
                saveImport.push_back(*m_token);
                m_token++;
                separator = std::get_if<Token::Separator>(&m_token->value);
            }
        } else {
            errorDesc("No file to import given in import statement.", "An import statement expects a path to a file: import path.to.file; or import \"path/to/file\".");
            return;
        }
        std::string alias;
        if (auto *keyword = std::get_if<Token::Keyword>(&m_token->value); keyword != nullptr && *keyword == Token::Keyword::As) {
            saveImport.push_back(*m_token);
            m_token++;
            if (m_token->type != Token::Type::Identifier) {
                error("Expected identifier after 'as' in import statement.");
                return;
            }
            alias = std::get_if<Token::Identifier>(&m_token->value)->get();
            saveImport.push_back(*m_token);
            m_token++;
        }

        auto *separator = std::get_if<Token::Separator>(&m_token->value);
        if (separator == nullptr || *separator != Token::Separator::Semicolon) {
            errorDesc("No semicolon found after import statement.", "An import statement expects a semicolon at the end: import path.to.file;");
            return;
        }
        saveImport.push_back(*m_token);
        m_token++;
        if(!m_resolver) {
            errorDesc("Unable to lookup results", "No include resolver was set.");
            return;
        }

        auto [resolved, error] = this->m_resolver(path);

        if(!resolved.has_value()) {
            for (const auto &item: error) {
                this->error(item);
            }
            return;
        }
        // determine if we should include this file
        if (this->m_onceIncludedFiles.contains({resolved.value(),alias}))
            return;


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
            auto [iter, added] = this->m_onceIncludedFiles.insert({resolved.value(), alias});
            if (!added) {
                shouldInclude = false;
            }
        }

        std::ranges::copy(preprocessor.m_onceIncludedFiles.begin(), preprocessor.m_onceIncludedFiles.end(), std::inserter(this->m_onceIncludedFiles, this->m_onceIncludedFiles.begin()));

        if (shouldInclude) {
          for (auto entry : saveImport) {
              m_output.push_back(entry);
          }
        }
        nextLine(line);
    }

    void Preprocessor::process() {
        u32 line = m_token->location.line;

        if (auto *directive = std::get_if<Token::Directive>(&m_token->value); directive != nullptr ) {
            auto handler = m_directiveHandlers.find(*directive);
            if (handler == m_directiveHandlers.end()) {
                error("Unknown directive '{}'", m_token->getFormattedValue());
                m_token++;
                return;
            } else {
                m_token++;
                handler->second(this, line);
                nextLine(line);
            }

        } else  if (auto *statement = std::get_if<Token::Keyword>(&m_token->value); statement != nullptr && *statement == Token::Keyword::Import) {
            auto handler = m_statementHandlers.find(*statement);
            if (handler == m_statementHandlers.end()) {
                error("Unknown statement '{}'", m_token->getFormattedValue());
                m_token++;
                return;
            } else {
                m_token++;
                handler->second(this, line);
                nextLine(line);
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
                message += std::string(" ") + tokenLiteral->toString(false);
                m_token++;
            }
            error(message);
        } else {
            error("No message given in #error directive.");
        }
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

    void Preprocessor::appendToNamespaces(std::vector<Token> tokens) {
        for (auto token = tokens.begin(); token != tokens.end(); token++ ) {
            u32 idx = 1;
            if (auto *keyword = std::get_if<Token::Keyword>(&token->value); keyword != nullptr && *keyword == Token::Keyword::Namespace) {
                if (auto *valueType = std::get_if<Token::ValueType>(&token[1].value);
                                                        valueType != nullptr && *valueType == Token::ValueType::Auto)
                    idx += 1;
                auto *identifier = std::get_if<Token::Identifier>(&token[idx].value);
                while (identifier != nullptr) {
                    if (auto *separator = std::get_if<Token::Separator>(&token[idx].value);
                                                        separator != nullptr && *separator == Token::Separator::EndOfProgram)
                        break;
                    if (std::ranges::find(m_namespaces, identifier->get()) == m_namespaces.end())
                        m_namespaces.push_back(identifier->get());
                    idx += 1;
                    if (auto *operatorToken = std::get_if<Token::Operator>(&token[idx].value);
                                                        operatorToken == nullptr || *operatorToken != Token::Operator::ScopeResolution)
                        break;
                    idx += 1;
                    identifier = std::get_if<Token::Identifier>(&token[idx].value);
                }
            }
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
            this->m_keys.clear();
            this->m_onlyIncludeOnce = false;

            this->m_defines.clear();
            for (const auto& [name, value] : m_runtime->getDefines()) {
                addDefine(name, value);
            }

            if (!source->mainSource) {
                addDefine("IMPORTED");
            }

            this->m_pragmas.clear();
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

        appendToNamespaces(m_output);

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

    void Preprocessor::addStatementHandler(const Token::Keyword &statementType, const api::StatementHandler &handler) {
        this->m_statementHandlers[statementType] = handler;
    }

    void Preprocessor::removePragmaHandler(const std::string &pragmaType) {
        this->m_pragmaHandlers.erase(pragmaType);
    }

    void Preprocessor::removeDirectiveHandler(const Token::Directive &directiveType) {
        this->m_directiveHandlers.erase(directiveType);
    }

    Location Preprocessor::location() {
        if (isInitialized()) {
            if (m_result.empty())
                return { nullptr, 0, 0, 0 };

            auto token = m_token;
            if (token == m_result.end()) {
                token = std::prev(token);
            }

            return token->location;
        } else
            return { nullptr, 0, 0, 0 };
    }
    void Preprocessor::registerStatementHandler(const Token::Keyword &name, auto memberFunction) {
        this->m_statementHandlers[name] = [memberFunction](Preprocessor* preprocessor, u32 line){
            (preprocessor->*memberFunction)(line);
        };
    }

    void Preprocessor::registerDirectiveHandler(const Token::Directive& name, auto memberFunction) {
        this->m_directiveHandlers[name] = [memberFunction](Preprocessor* preprocessor, u32 line){
            (preprocessor->*memberFunction)(line);
        };
    }

}