#include <pl/pattern_language.hpp>
#include <pl/core/parser_manager.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/lexer.hpp>

#include <wolv/utils/string.hpp>

using namespace pl::core;

pl::hlp::CompileResult<ParserManager::ParsedData> ParserManager::parse(api::Source *source, const std::string &namespacePrefix) {
    using result_t = hlp::CompileResult<ParsedData>;

    if (m_onceIncluded.contains( { source, namespacePrefix } ))
        return result_t::good({});

    auto parser = Parser();

    std::vector<std::string> namespaces;
    if (!namespacePrefix.empty()) {
        namespaces = wolv::util::splitString(namespacePrefix, "::");
    }

    const auto& internals = m_patternLanguage->getInternals();

    const auto& preprocessor = internals.preprocessor;

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
    parser.setDefaultNamespace(namespaces);

    auto result = parser.parse(tokens.value());
    if (result.hasErrs())
        return result_t::err(result.errs);

    // if its ok validate before returning
    auto [validated, validatorErrors] = validator->validate(*result.ok);
    if (validated && !validatorErrors.empty()) {
        return result_t::err(validatorErrors);
    }

    ParsedData parsedData = {
        .astNodes = result.unwrap(),
        .types = parser.getTypes()
    };

    return result_t::good(std::move(parsedData));
}