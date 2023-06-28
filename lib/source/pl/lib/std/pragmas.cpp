#include <pl.hpp>

#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/core/errors/preprocessor_errors.hpp>

using namespace pl;

std::optional<u64> parseLimit(const std::string &value) {
    try {
        size_t index = 0;
        auto limit = std::stoull(value, &index, 0);
        if (index != value.size())
            return std::nullopt;

        if (limit == 0)
            return std::numeric_limits<u64>::max();
        else
            return limit;
    } catch (std::exception &) {
        return std::nullopt;
    }
}

namespace pl::lib::libstd {
    void registerPragmas(pl::PatternLanguage &runtime) {
        runtime.addPragma("endian", [](pl::PatternLanguage &runtime, const std::string &value) {
            if (value == "big") {
                runtime.getInternals().evaluator->setDefaultEndian(std::endian::big);
                return true;
            } else if (value == "little") {
                runtime.getInternals().evaluator->setDefaultEndian(std::endian::little);
                return true;
            } else if (value == "native") {
                runtime.getInternals().evaluator->setDefaultEndian(std::endian::native);
                return true;
            } else
                return false;
        });

        runtime.addPragma("eval_depth", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = parseLimit(value);
            if (!limit.has_value())
                return false;

            runtime.getInternals().evaluator->setEvaluationDepth(*limit);
            runtime.getInternals().validator->setRecursionDepth(*limit);
            return true;
        });

        runtime.addPragma("array_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = parseLimit(value);
            if (!limit.has_value())
                return false;

            runtime.getInternals().evaluator->setArrayLimit(*limit);
            return true;
        });

        runtime.addPragma("pattern_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = parseLimit(value);
            if (!limit.has_value())
                return false;

            runtime.getInternals().evaluator->setPatternLimit(*limit);
            return true;
        });

        runtime.addPragma("loop_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = parseLimit(value);
            if (!limit.has_value())
                return false;

            runtime.getInternals().evaluator->setLoopLimit(*limit);
            return true;
        });

        runtime.addPragma("bitfield_order", [](pl::PatternLanguage &, const std::string &) -> bool {
            core::err::M0006.throwError("Pragma 'bitfield_order' is unsupported.",
                "Bitfield order can be overridden on a field declaration with the `be` or `le` keywords.");
        });

        runtime.addPragma("debug", [](pl::PatternLanguage &runtime, const std::string &value) {
            if (!value.empty())
                return false;

            runtime.getInternals().evaluator->setDebugMode(true);

            return true;
        });

    }

}
