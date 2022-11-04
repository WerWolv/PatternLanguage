#include <pl.hpp>

#include <pl/core/validator.hpp>
#include <pl/core/evaluator.hpp>

const long unsigned int LIMITMAX = std::numeric_limits<long unsigned int>::max() - 1;

bool parse_limit(const std::string &value, long unsigned int *limit) {
    char* s = (char*)value.c_str();
    char** endptr = &s;
    auto tlimit = strtol(s, endptr, 0);

    if (endptr != nullptr && **endptr != 0) {
        return false;
    } else if (tlimit == 0) {
        *limit = LIMITMAX;
    } else {
        *limit = tlimit;
    }
    return true;
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
            long unsigned int limit = 0;
            bool parsed = parse_limit(value, &limit);

            if (!parsed || limit == LIMITMAX)
                return false;

            runtime.getInternals().evaluator->setEvaluationDepth(limit);
            runtime.getInternals().validator->setRecursionDepth(limit);
            return true;
        });

        runtime.addPragma("array_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            long unsigned int limit = 0;
            bool parsed = parse_limit(value, &limit);

            if (!parsed)
                return false;

            runtime.getInternals().evaluator->setArrayLimit(limit);
            return true;
        });

        runtime.addPragma("pattern_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            long unsigned int limit = 0;
            bool parsed = parse_limit(value, &limit);

            if (!parsed)
                return false;

            runtime.getInternals().evaluator->setPatternLimit(limit);
            return true;
        });

        runtime.addPragma("loop_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            long unsigned int limit = 0;
            if (!parse_limit(value, &limit) || limit == LIMITMAX)
                return false;

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
