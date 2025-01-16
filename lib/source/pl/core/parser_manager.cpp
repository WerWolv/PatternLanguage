#include <pl/pattern_language.hpp>
#include <pl/core/parser_manager.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/validator.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>

#include <wolv/utils/string.hpp>

namespace pl::core {

    using Result = hlp::CompileResult<ParserManager::ParsedData>;
    Result ParserManager::parse(api::Source *source, const std::string &namespacePrefix) {
        OnceIncludePair key = { source, namespacePrefix };

        if (m_onceIncluded.contains(key)) {
            const auto& types = m_parsedTypes[key];
            return Result::good({ {}, types }); // Even if types is empty we still need to return now
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

    ast::ASTNodeTypeDecl* ParserManager::addBuiltinType(const std::string &name, api::FunctionParameterCount parameterCount, const api::TypeCallback &func) {
        auto type = this->m_builtinTypes.emplace(name,
            hlp::safe_shared_ptr<ast::ASTNodeTypeDecl>(
                new ast::ASTNodeTypeDecl(name, std::make_shared<ast::ASTNodeBuiltinType>(parameterCount, func))
            )
        ).first->second.get();

        if (parameterCount.min != parameterCount.max)
            throw std::runtime_error("Types cannot have a variable amount of parameters");

        std::vector<std::shared_ptr<ast::ASTNode>> templateParameters;
        for (u32 i = 0; i < parameterCount.max; i += 1) {
            templateParameters.emplace_back(std::make_shared<ast::ASTNodeLValueAssignment>(fmt::format("$param{}$", i), nullptr));
        }
        type->setTemplateParameters(std::move(templateParameters));

        return type;
    }

}

