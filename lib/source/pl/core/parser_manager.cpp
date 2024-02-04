#include <pl/pattern_language.hpp>
#include <pl/core/parser_manager.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/lexer.hpp>

#include <wolv/utils/string.hpp>

using namespace pl::core;

pl::hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>>
ParserManager::parse(api::Source *source, const std::string &namespacePrefix) {
    using result_t = hlp::CompileResult<std::vector<std::shared_ptr<ast::ASTNode>>>;

    if(m_onceIncluded.contains( { source, namespacePrefix } ))
        return result_t::good({});

    auto parser = Parser();

    std::vector<std::string> namespaces;
    if (!namespacePrefix.empty()) {
        namespaces = wolv::util::splitString(namespacePrefix, "::");
    }

    const auto& internals = m_patternLanguage->getInternals();

    const auto& preprocessor = internals.preprocessor;

    preprocessor->addPragmaHandler("namespace", [&](auto&, const std::string& value) {
        if(namespacePrefix.empty())
            namespaces = wolv::util::splitString(value, "::");
        return true;
    });

    const auto& lexer = internals.lexer;
    const auto& validator = internals.validator;

    auto [preprocessedCode, preprocessorErrors] = preprocessor->preprocess(this->m_patternLanguage, source);
    if (!preprocessorErrors.empty()) {
        return result_t::err(preprocessorErrors);
    }

    source->content = preprocessedCode.value();

    if(preprocessor->shouldOnlyIncludeOnce())
        m_onceIncluded.insert( { source, namespacePrefix } );

    auto [tokens, lexerErrors] = lexer->lex(source);

    if (!lexerErrors.empty()) {
        return result_t::err(lexerErrors);
    }

    parser.setParserManager(this);

    auto result = parser.parse(tokens.value(), namespaces);
    if (result.hasErrs())
        return result;

    // if its ok validate before returning
    auto [validated, validatorErrors] = validator->validate(*result.ok);
    if (validated && !validatorErrors.empty()) {
        return result_t::err(validatorErrors);
    }

    return result;
}