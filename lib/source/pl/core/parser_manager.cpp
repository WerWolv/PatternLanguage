#include <pl/pattern_language.hpp>
#include <pl/core/parser_manager.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/lexer.hpp>

#include <wolv/utils/string.hpp>

using namespace pl::core;

using Result = pl::hlp::CompileResult<ParserManager::ParsedData>;
Result ParserManager::parse(api::Source *source, const std::string &namespacePrefix) {
    OnceIncludePair key = { source, namespacePrefix };

    if (m_onceIncluded.contains( key )) {
        const auto& types = m_parsedTypes[key];
        if (!types.empty())
            return Result::good({ {}, types });
    }

    Parser parser;

    std::vector<std::string> namespaces;
    if (!namespacePrefix.empty()) {
        namespaces = wolv::util::splitString(namespacePrefix, "::");
    }

    const auto &internals = m_patternLanguage->getInternals();
    auto oldPreprocessor = internals.preprocessor.get();

    Preprocessor preprocessor;
    preprocessor.setResolver(m_resolver);

    // Add a define that's used to determine if a file is imported
    preprocessor.addDefine("IMPORTED");

    for (const auto& [name, value] : m_patternLanguage->getDefines()) {
        preprocessor.addDefine(name, value);
    }
    for (const auto& [name, handler]: m_patternLanguage->getPragmas()) {
        preprocessor.addPragmaHandler(name, handler);
    }

    const auto &validator = internals.validator;

    auto [tokens, preprocessorErrors] = preprocessor.preprocess(this->m_patternLanguage, source, true);
    if (!preprocessorErrors.empty()) {
        return Result::err(preprocessorErrors);
    }

    if (preprocessor.shouldOnlyIncludeOnce())
        m_onceIncluded.insert( { source, namespacePrefix } );

    parser.m_parserManager = this;
    parser.m_aliasNamespace = namespaces;
    parser.m_aliasNamespaceString = namespacePrefix;

    auto result = parser.parse(tokens.value());
    oldPreprocessor->appendToNamespaces(tokens.value());

    if (result.hasErrs())
        return Result::err(result.errs);

    auto [validated, validatorErrors] = validator->validate(result.ok.value());
    if (validated && !validatorErrors.empty()) {
        return Result::err(validatorErrors);
    }

    // 'Uncomplete' all types
    const auto &types = parser.m_types;
    for (auto &[name, type] : types) {
        type->setCompleted(false);
    }

    m_parsedTypes[key] = types;

    return Result::good({ result.unwrap(), types });
}