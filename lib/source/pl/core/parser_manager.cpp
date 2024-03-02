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

    OnceIncludePair key = { source, namespacePrefix };

    if (m_onceIncluded.contains( key )) {
        const auto& types = m_parsedTypes[key];
        return result_t::good({ {}, types });
    }

    auto parser = Parser();

    std::vector<std::string> namespaces;
    if (!namespacePrefix.empty()) {
        namespaces = wolv::util::splitString(namespacePrefix, "::");
    }

    const auto& internals = m_patternLanguage->getInternals();

    auto preprocessor = Preprocessor();
    for (const auto& [name, value] : m_patternLanguage->getDefines()) {
        preprocessor.addDefine(name, value);
    }
    for (const auto& [name, handler]: m_patternLanguage->getPragmas()) {
        preprocessor.addPragmaHandler(name, handler);
    }

    const auto& validator = internals.validator;

    auto [tokens, preprocessorErrors] = preprocessor.preprocess(this->m_patternLanguage, source, true);
    if (!preprocessorErrors.empty()) {
        return result_t::err(preprocessorErrors);
    }

    if(preprocessor.shouldOnlyIncludeOnce())
        m_onceIncluded.insert( { source, namespacePrefix } );

    parser.m_parserManager = this;
    parser.m_aliasNamespace = namespaces;
    parser.m_aliasNamespaceString = namespacePrefix;

    auto result = parser.parse(tokens.value());
    if (result.hasErrs())
        return result_t::err(result.errs);

    // if its ok validate before returning
    auto [validated, validatorErrors] = validator->validate(result.ok.value());
    if (validated && !validatorErrors.empty()) {
        return result_t::err(validatorErrors);
    }

    const auto& types = parser.m_types;
    for (auto& type : types) {
        type.second->setCompleted(false); // de-complete the types
    }

    m_parsedTypes[key] = types;

    return result_t::good({ result.unwrap(), types });
}