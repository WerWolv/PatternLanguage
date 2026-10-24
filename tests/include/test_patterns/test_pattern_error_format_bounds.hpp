#pragma once

#include "test_pattern.hpp"

#include <pl/api.hpp>
#include <pl/core/location.hpp>
#include <pl/core/errors/error.hpp>

namespace pl::test {

    // formatLines() renders the offending source line with a caret under the reported column. The
    // column is reported 1-based and can legitimately point past the end of its line, for instance
    // when a token is left unterminated at the end of it. It is exercised directly here rather than
    // through a malformed pattern, so that the coverage does not quietly disappear if the lexer's
    // column bookkeeping ever changes.
    class TestPatternErrorFormatBounds : public TestPattern {
    public:
        TestPatternErrorFormatBounds(core::Evaluator *evaluator) : TestPattern(evaluator, "ErrorFormatBounds") {
        }
        ~TestPatternErrorFormatBounds() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u8 placeholder @ 0x00;
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            wolv::util::unused(patterns);

            // Long enough to take the trimming path
            const api::Source longLine(std::string(64, 'x'), "<bounds test>");
            const api::Source shortLine("abc", "<bounds test>");

            // A column past the end of the line must still render, with the caret just after the
            // final character, both with and without the trimming path
            if (core::err::impl::formatLines({ &longLine, 1, 1000, 1 }).empty())
                return false;
            if (core::err::impl::formatLines({ &shortLine, 1, 999, 1 }).empty())
                return false;

            // Column zero must not wrap around when it is converted to a caret offset
            if (core::err::impl::formatLines({ &longLine, 1, 0, 1 }).empty())
                return false;

            // Locations that name no usable line render nothing instead of reaching for one
            if (!core::err::impl::formatLines({ &longLine, 0, 5, 1 }).empty())
                return false;
            if (!core::err::impl::formatLines({ &longLine, 99, 5, 1 }).empty())
                return false;
            if (!core::err::impl::formatLines(core::Location::Empty()).empty())
                return false;

            return true;
        }
    };

}
