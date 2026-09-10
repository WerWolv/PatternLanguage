#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_unsigned.hpp>

namespace pl::test {

    // Test that forEachEntrySorted works correctly after setEntries is called
    // without a preceding sort(). This tests the fix for the bug where
    // setFields() populates m_sortedFields, but a subsequent setEntries()
    // overwrites m_fields without updating m_sortedFields, leaving it stale.
    //
    // The crash scenario (template bitfield):
    //   1. ASTNodeBitfield::createPatterns() calls setFields() -> m_fields=2, m_sortedFields=2
    //   2. ASTNodeTypeDecl::createPatterns() calls setEntries(scope) ->
    //      m_fields=3 (original 2 + template param), m_sortedFields still 2 (stale!)
    //   3. forEachEntryImpl: i < end (=3) indexing into patterns (size 2) -> crash
    class TestPatternSetEntriesSortedConsistency : public TestPattern {
    public:
        TestPatternSetEntriesSortedConsistency(core::Evaluator *evaluator) : TestPattern(evaluator, "SetEntriesSortedConsistency") {
        }
        ~TestPatternSetEntriesSortedConsistency() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                bitfield TemplateBitfield<auto N> {
                    unsigned a : 2;
                    unsigned b : 3;
                };

                TemplateBitfield<4> bf @ 0;
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            for (const auto &pattern : patterns) {
                auto iterable = dynamic_cast<ptrn::IIterable *>(pattern.get());
                if (iterable == nullptr)
                    continue;

                size_t entryCount = iterable->getEntryCount();
                if (entryCount == 0)
                    continue;

                // forEachEntrySorted and forEachEntry must produce the same
                // number of non-local entries. Before the fix, forEachEntrySorted
                // would crash (stale sorted entries) or return fewer entries.
                size_t sortedCount = 0;
                iterable->forEachEntrySorted(0, entryCount,
                    [&](u64, const auto &) { sortedCount++; });

                size_t unsortedCount = 0;
                iterable->forEachEntry(0, entryCount,
                    [&](u64, const auto &) { unsortedCount++; });

                if (sortedCount != unsortedCount)
                    return false;
            }

            return true;
        }
    };

}
