#include <map>
#include <string>
#include <cstdlib>

#include <pl/helpers/utils.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/core/ast/ast_node.hpp>
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

    wolv::io::File testData("test_data", wolv::io::File::Mode::Read);
    pl::PatternLanguage runtime;
    runtime.setDataSource(0x00, testData.getSize(), [&testData](u64 offset, u8 *buffer, u64 size) {
        testData.seek(offset);
        testData.readBuffer(buffer, size);
    });


    runtime.addFunction({ "std" }, "assert", api::FunctionParameterCount::exactly(2), [](core::Evaluator *ctx, auto params) -> std::optional<core::Token::Literal> {
        auto condition = params[0].toBoolean();
        auto message   = params[1].toString(false);

        if (!condition)
            core::err::E0012.throwError(fmt::format("assertion failed \"{0}\"", message));

        return std::nullopt;
    });

    runtime.setLogCallback([](auto level, const std::string &message) {
        fmt::print("{}\n", message);
    });

    // Include test
    (void)runtime.addVirtualSource(R"(
        #include <B>
        #include <C>

        fn a() {};
    )", "A");

    (void)runtime.addVirtualSource(R"(
        #include <C>

        fn b() {};
    )", "B");

    (void)runtime.addVirtualSource(R"(
        #pragma once

        fn c() {};
    )", "C");

    // Imports test
    (void)runtime.addVirtualSource(R"(
        import IB;
        import IC as C;

        fn a() {};
    )", "IA");

    (void)runtime.addVirtualSource(R"(
        // auto means will be skipped if alias is set, effectively acting as a fallback
        namespace auto B {

            import IC as C;

            fn b() {};

        }
    )", "IB");

    (void)runtime.addVirtualSource(R"(
        #pragma once

        fn c() {};
    )", "IC");

    auto &test = testPatterns[testName];
    test->m_runtime = &runtime;
    test->setup();
    auto result = runtime.executeString(test->getSourceCode());

    // Check if compilation succeeded
    if (!result) {
        fmt::print("Error during test!\n");

        if (auto error = runtime.getEvalError(); error.has_value())
            fmt::print("Error: {}:{} : {}\n", error->line, error->column, error->message);

        for (const auto& error : runtime.getCompileErrors()) {
            fmt::print("{}", error.format());
        }

        return failing ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const auto &evaluatedPatterns = runtime.getPatterns();
    const auto &controlPatterns   = currTest->getPatterns();

    if (!test->runChecks(evaluatedPatterns)) {
        fmt::print("Post-run checks failed!\n");

        return failing ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (failing) {
        fmt::print("Failing test succeeded!\n");
        return EXIT_FAILURE;
    }

    // Check if the right number of patterns have been produced
    if (evaluatedPatterns.size() != controlPatterns.size() && !controlPatterns.empty()) {
        fmt::print("Source didn't produce expected number of patterns\n");
        return EXIT_FAILURE;
    }

    // Check if the produced patterns are the ones expected
    for (u32 i = 0; i < std::min(evaluatedPatterns.size(), controlPatterns.size()); i++) {
        auto &evaluatedPattern = *evaluatedPatterns[i];
        auto &controlPattern   = *controlPatterns[i];

        if (evaluatedPattern != controlPattern) {
            fmt::print("Pattern with name {} didn't match template\n", evaluatedPattern.getVariableName());
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int result = EXIT_SUCCESS;

    for (u32 i = 0; i < 16; i++) { // Test several times with the same PatternLanguage runtime instance (see static declaration in runTests()) to check if the runtime resets properly
        result = runTests(argc, argv);
        if (result != EXIT_SUCCESS)
            break;
    }

    ON_SCOPE_EXIT {
        for (auto &[key, value] : TestPattern::getTests())
            delete value;
    };

    if (result == EXIT_SUCCESS)
        fmt::print("Success!\n");
    else
        fmt::print("Failed!\n");

    return result;
}