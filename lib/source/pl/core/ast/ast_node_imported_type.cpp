#include <pl/core/evaluator.hpp>
#include <pl/core/ast/ast_node_imported_type.hpp>
#include <pl/patterns/pattern_struct.hpp>

namespace pl::core::ast {

    ASTNodeImportedType::ASTNodeImportedType(const std::string &importedTypeName) : m_importedTypeName(importedTypeName) {

    }

    void ASTNodeImportedType::createPatterns(Evaluator *evaluator, std::vector<std::shared_ptr<ptrn::Pattern>> &resultPatterns) const {
        auto &runtime = evaluator->createSubRuntime();
        auto resolver = runtime.getResolver().resolve(m_importedTypeName);

        if (resolver.hasErrs()) {
            err::E0005.throwError(resolver.unwrapErrs().front());
        }

        auto source = resolver.unwrap();

        const auto startAddress = evaluator->getReadOffset();
        const auto sectionId = evaluator->getSectionId();
        const bool customSection = sectionId != ptrn::Pattern::MainSectionId
            && sectionId != ptrn::Pattern::HeapSectionId
            && sectionId != ptrn::Pattern::PatternLocalSectionId
            && sectionId != ptrn::Pattern::InstantiationSectionId;

        if (customSection) {
            runtime.setDataSource(0x00, evaluator->getSectionSize(sectionId),
                [evaluator, sectionId](u64 address, u8 *buffer, size_t size) {
                    evaluator->readData(address, buffer, size, sectionId);
                },
                [evaluator, sectionId](u64 address, const u8 *buffer, size_t size) {
                    evaluator->writeData(address, const_cast<u8*>(buffer), size, sectionId);
                }
            );
            runtime.setStartAddress(startAddress);
        } else {
            runtime.setStartAddress(evaluator->getStartAddress() + startAddress);
        }

        if (runtime.executeString(source->content, source->source) != 0) {
            err::E0005.throwError(fmt::format("Error while processing imported type '{}'.", m_importedTypeName), "Check the imported pattern for errors.", getLocation());
        }

        auto patterns = runtime.getPatterns();
        if (customSection) {
            for (const auto &pattern : patterns)
                evaluator->changePatternSection(pattern.get(), sectionId);
        }

        auto &result = resultPatterns.emplace_back();
        if (patterns.size() == 1) {
            auto &pattern = patterns.front();
            pattern->setTypeName(m_importedTypeName);

            result = std::move(pattern);
        } else {
            auto structPattern = std::make_shared<ptrn::PatternStruct>(evaluator, 0x00, 0, getLocation().line);

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
    }

    std::unique_ptr<ASTNode> ASTNodeImportedType::evaluate(Evaluator *) const {
        err::E0005.throwError("Cannot use imported type in evaluated context.");
    }


}
