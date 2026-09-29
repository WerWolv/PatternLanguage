#include "test_patterns/test_pattern_format_heap.hpp"

namespace pl::test {

    std::shared_ptr<ptrn::Pattern> findPattern(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns, const std::string &name) {
        for (const auto &pattern : patterns) {
            if (pattern->getVariableName() == name)
                return pattern;
        }

        return nullptr;
    }

    void forEachLeaf(const std::shared_ptr<ptrn::Pattern> &pattern, const std::function<void(ptrn::Pattern&)> &callback) {
        if (auto iterable = dynamic_cast<ptrn::IIterable*>(pattern.get()); iterable != nullptr) {
            iterable->forEachEntry(0, iterable->getEntryCount(), [&](u64, const auto &entry) {
                forEachLeaf(entry, callback);
            });
        } else {
            callback(*pattern);
        }
    }

}
