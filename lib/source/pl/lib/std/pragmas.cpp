#include <pl.hpp>

#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>

using namespace pl;

constexpr static auto MaxLimit = std::numeric_limits<u64>::max();

std::optional<u64> parseLimit(const std::string &value) {
    std::size_t pos {};
    auto tlimit = std::stoll(value, &pos);

    if (pos == 0) {
        return std::nullopt;
    } else if (tlimit == 0) {
        tlimit = MaxLimit;
    }
    return std::optional<u64>{tlimit};
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
            auto tlimit = parseLimit(value);
            if (!tlimit.has_value() || tlimit == std::optional{MaxLimit})
                return false;
            auto limit = tlimit.value();

            runtime.getInternals().evaluator->setEvaluationDepth(limit);
            runtime.getInternals().validator->setRecursionDepth(limit);
            return true;
        });

        runtime.addPragma("array_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto tlimit = parseLimit(value);
            if (!tlimit.has_value())
                return false;
            auto limit = tlimit.value();

            runtime.getInternals().evaluator->setArrayLimit(limit);
            return true;
        });

        runtime.addPragma("pattern_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto tlimit = parseLimit(value);
            if (!tlimit.has_value())
                return false;
            auto limit = tlimit.value();

            runtime.getInternals().evaluator->setPatternLimit(limit);
            return true;
        });

        runtime.addPragma("loop_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto tlimit = parseLimit(value);
            if (!tlimit.has_value() || tlimit == std::optional{MaxLimit})
                return false;
            auto limit = tlimit.value();

            runtime.getInternals().evaluator->setLoopLimit(limit);
            return true;
        });

        runtime.addPragma("bitfield_order", [](pl::PatternLanguage &runtime, const std::string &value) {
            if (value == "left_to_right") {
                runtime.getInternals().evaluator->setBitfieldOrder(pl::core::BitfieldOrder::LeftToRight);
                return true;
            } else if (value == "right_to_left") {
                runtime.getInternals().evaluator->setBitfieldOrder(pl::core::BitfieldOrder::RightToLeft);
                return true;
            } else {
                return false;
            }
        });

        runtime.addPragma("debug", [](pl::PatternLanguage &runtime, const std::string &value) {
            if (!value.empty())
                return false;

            runtime.getInternals().evaluator->setDebugMode(true);

            return true;
        });

    }

}
