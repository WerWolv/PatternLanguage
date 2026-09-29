#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern.hpp>

#include <chrono>
#include <functional>

namespace pl::test {

    /**
     * @brief Finds a top-level pattern by its variable name
     * @param patterns Patterns to search
     * @param name Variable name to look for
     * @return The pattern, or nullptr if no pattern has that name
     */
    std::shared_ptr<ptrn::Pattern> findPattern(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns, const std::string &name);

    /**
     * @brief Calls a function on every pattern inside a pattern that has no
     * entries of its own, such as the members of each struct in an array
     * @param pattern Pattern to walk through
     * @param callback Function to call on each of these patterns
     */
    void forEachLeaf(const std::shared_ptr<ptrn::Pattern> &pattern, const std::function<void(ptrn::Pattern&)> &callback);

    class TestPatternFormatHeapGrowth : public TestPattern {
    public:
        TestPatternFormatHeapGrowth(core::Evaluator *evaluator) : TestPattern(evaluator, "FormatHeapGrowth") {
        }
        ~TestPatternFormatHeapGrowth() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                str filler = "0123456789ABCDEF" * 0x100000;

                fn format_entry(auto value) {
                    return "x";
                };

                struct Entry {
                    u8 value [[format("format_entry")]];
                };

                Entry entries[0x4000] @ 0x00;
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            auto entries = findPattern(patterns, "entries");
            if (entries == nullptr)
                return false;

            // Each formatter call used to copy the whole heap, which made this take about 20 seconds
            const auto start = std::chrono::steady_clock::now();

            bool valid = true;
            forEachLeaf(entries, [&](ptrn::Pattern &pattern) {
                if (pattern.getFormattedValue() != "x")
                    valid = false;
            });

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            if (elapsed > std::chrono::seconds(5)) {
                fmt::print("Formatting took {} ms\n", elapsed.count());
                return false;
            }

            return valid;
        }
    };

    class TestPatternFormatHeapIsolation : public TestPattern {
    public:
        TestPatternFormatHeapIsolation(core::Evaluator *evaluator) : TestPattern(evaluator, "FormatHeapIsolation") {
        }
        ~TestPatternFormatHeapIsolation() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 counter = 0;
                str label = "a";

                fn bump_format(auto value) {
                    counter = counter + 1;
                    label = "b";
                    return "bumped";
                };

                fn bump_transform(auto value) {
                    counter = counter + 100;
                    label = "c";
                    return value;
                };

                fn show_state(auto value) {
                    if (counter == 0 && label == "a")
                        return "clean";
                    else
                        return "leaked";
                };

                struct Formatted {
                    u8 value [[format("bump_format")]];
                };

                struct Transformed {
                    u8 value [[transform("bump_transform")]];
                };

                Formatted formatted[4] @ 0x00;
                Transformed transformed[4] @ 0x10;
                u8 state @ 0x20 [[format("show_state")]];
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            auto formatted = findPattern(patterns, "formatted");
            auto transformed = findPattern(patterns, "transformed");
            auto state = findPattern(patterns, "state");
            if (formatted == nullptr || transformed == nullptr || state == nullptr)
                return false;

            bool valid = true;
            forEachLeaf(formatted, [&](ptrn::Pattern &pattern) {
                if (pattern.getFormattedValue() != "bumped")
                    valid = false;
            });
            forEachLeaf(transformed, [&](ptrn::Pattern &pattern) {
                wolv::util::unused(pattern.getValue());
            });

            // Changes that formatters and transforms make to globals must not last after they return
            if (auto stateValue = state->getFormattedValue(); stateValue != "clean") {
                fmt::print("Formatter state leaked: {}\n", stateValue);
                valid = false;
            }

            return valid;
        }
    };

}
