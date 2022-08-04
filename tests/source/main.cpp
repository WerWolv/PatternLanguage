#include <map>
#include <string>
#include <cstdlib>

#include <helpers/utils.hpp>
#include <helpers/file.hpp>

#include <pl/pattern_language.hpp>
#include <pl/evaluator.hpp>
#include <pl/ast/ast_node.hpp>
#include <pl/patterns/pattern.hpp>

#include "test_patterns/test_pattern.hpp"

#include <fmt/args.h>

using namespace pl;
using namespace pl::test;

int runTests(int argc, char **argv) {
    auto &testPatterns = TestPattern::getTests();

    // Check if a test to run has been provided
    if (argc != 2) {
        fmt::print("Invalid number of arguments specified! {}\n", argc);
        return EXIT_FAILURE;
    }

    // Check if that test exists
    std::string testName = argv[1];
    if (!testPatterns.contains(testName)) {
        fmt::print("No test with name {} found!\n", testName);
        return EXIT_FAILURE;
    }

    const auto &currTest = testPatterns[testName];
    bool failing         = currTest->getMode() == Mode::Failing;

    fs::File testData("test_data", fs::File::Mode::Read);
    pl::PatternLanguage runtime;
    runtime.setDataSource([&testData](u64 offset, u8 *buffer, u64 size) {
        testData.seek(offset);
        testData.readBuffer(buffer, size);
    }, 0x00, testData.getSize());


    runtime.addFunction({ "std" }, "assert", api::FunctionParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
        auto condition = Token::literalToBoolean(params[0]);
        auto message   = Token::literalToString(params[1], false);

        if (!condition)
            pl::err::E0012.throwError(fmt::format("assertion failed \"{0}\"", message));

        return std::nullopt;
    });

    // Check if compilation succeeded
    auto result = runtime.executeString(testPatterns[testName]->getSourceCode());
    if (!result) {
        fmt::print("Error during compilation!\n");

        if (auto error = runtime.getError(); error.has_value())
            fmt::print("Compile error: {}:{} : {}\n", error->getLine(), error->getColumn(), error->what());
        for (auto &[level, message] : runtime.getConsoleLog())
            fmt::print("Evaluate error: {}\n", message);

        return failing ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (failing) {
        fmt::print("Failing test succeeded!\n");
        return EXIT_FAILURE;
    }

    // Check if the right number of patterns have been produced
    if (runtime.getPatterns().size() != currTest->getPatterns().size() && !currTest->getPatterns().empty()) {
        fmt::print("Source didn't produce expected number of patterns\n");
        return EXIT_FAILURE;
    }

    // Check if the produced patterns are the ones expected
    for (u32 i = 0; i < currTest->getPatterns().size(); i++) {
        auto &evaluatedPattern = *runtime.getPatterns()[i];
        auto &controlPattern   = *currTest->getPatterns()[i];

        if (evaluatedPattern != controlPattern) {
            fmt::print("Pattern with name {}:{} didn't match template\n", evaluatedPattern.getTypeName(), evaluatedPattern.getVariableName());
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int result = EXIT_SUCCESS;

    for (u32 i = 0; i < 16; i++) {
        result = runTests(argc, argv);
        if (result != EXIT_SUCCESS)
            break;
    }

    PL_ON_SCOPE_EXIT {
        for (auto &[key, value] : TestPattern::getTests())
            delete value;
    };

    if (result == EXIT_SUCCESS)
        fmt::print("Success!\n");
    else
        fmt::print("Failed!\n");

    return result;
}