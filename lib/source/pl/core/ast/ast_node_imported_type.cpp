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

        const auto startAddress = evaluator->getReadOffset();
        runtime.setStartAddress(startAddress);
        if (!runtime.executeString(source->content, source->source)) {
            err::E0005.throwError(fmt::format("Error while processing imported type '{}'.", m_importedTypeName), "Check the imported pattern for errors.", getLocation());
        }

        auto patterns = runtime.getPatterns();

        std::shared_ptr<ptrn::Pattern> result;
        if (patterns.size() == 1) {
            auto &pattern = patterns.front();
            pattern->setTypeName(m_importedTypeName);

            result = std::move(pattern);
        } else {
            auto structPattern = std::make_shared<ptrn::PatternStruct>(evaluator, evaluator->getReadOffset(), 0, getLocation().line);

            u64 minPos = std::numeric_limits<u64>::max();
            u64 maxPos = std::numeric_limits<u64>::min();

            for (const auto &pattern : patterns) {
                minPos = std::min(minPos, pattern->getOffset());
                maxPos = std::max(maxPos, pattern->getOffset() + pattern->getSize());
            }

            structPattern->setEntries(patterns);
            structPattern->setSize(maxPos - minPos);

            result = std::move(structPattern);
        }

        result->setOffset(startAddress);
        runtime.setStartAddress(evaluator->getRuntime().getStartAddress());

        evaluator->setReadOffset(startAddress + result->getSize());

        return { result };
    }

    std::unique_ptr<ASTNode> ASTNodeImportedType::evaluate(Evaluator *) const {
        err::E0005.throwError("Cannot use imported type in evaluated context.");
    }


}
