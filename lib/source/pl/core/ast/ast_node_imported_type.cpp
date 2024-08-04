#include <pl/core/evaluator.hpp>
#include <pl/core/ast/ast_node_imported_type.hpp>
#include <pl/patterns/pattern_struct.hpp>

namespace pl::core::ast {

    ASTNodeImportedType::ASTNodeImportedType(const std::string &importedTypeName) : m_importedTypeName(importedTypeName) {

    }

    std::vector<std::shared_ptr<ptrn::Pattern>> ASTNodeImportedType::createPatterns(Evaluator *evaluator) const {
        auto &runtime = evaluator->createSubRuntime();
        auto resolver = runtime.getResolver().resolve(m_importedTypeName);

        if (resolver.hasErrs()) {
            err::E0005.throwError(resolver.unwrapErrs().front());
        }

        auto source = resolver.unwrap();

        runtime.setStartAddress(evaluator->getReadOffset());
        if (!runtime.executeString(source->content, source->source)) {
            err::E0005.throwError(fmt::format("Error while processing imported type '{}'.", m_importedTypeName), "Check the imported pattern for errors.", getLocation());
        }

        auto patterns = runtime.getPatterns();
        if (patterns.size() == 1) {
            patterns.front()->setTypeName(m_importedTypeName);
            return patterns;
        } else {
            auto structPattern = std::make_shared<ptrn::PatternStruct>(evaluator, evaluator->getReadOffset(), 0, getLocation().line);
            structPattern->setMembers(std::move(patterns));

            return { structPattern };
        }

    }

    std::unique_ptr<ASTNode> ASTNodeImportedType::evaluate(Evaluator *) const {
        err::E0005.throwError("Cannot use imported type in evaluated context.");
    }


}
