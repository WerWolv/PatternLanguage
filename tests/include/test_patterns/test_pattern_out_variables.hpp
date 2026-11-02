#pragma once

#include "test_pattern.hpp"
#include "test_pattern_read_string.hpp"

#include <pl/pattern_language.hpp>

namespace pl::test {

    /**
     * @brief A str out variable keeps the text assigned to it under a non-UTF-8 #pragma encoding
     *
     * Under "ascii" the test codec rejects a byte above 0x7F. A str out variable must not go
     * through it, because it holds text, not data bytes.
     */
    class TestPatternOutVariableEncoding : public TestPattern {
    public:
        TestPatternOutVariableEncoding(core::Evaluator *evaluator) : TestPattern(evaluator, "OutVariableEncoding") { }
        ~TestPatternOutVariableEncoding() override = default;

        void setup() override {
            m_runtime->setStringEncodeDecode(std::make_shared<ReadStringCodec>());
        }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #pragma encoding ascii

                str text out;
                str raw out;
                text = "café";
                raw = builtin::std::mem::read_string(0, 4, 0, "UTF-8");
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &) const override {
            const auto outVariables = m_runtime->getOutVariables();

            if (outVariables.at("text").toString(false) != "caf\xC3\xA9")
                throw std::runtime_error(fmt::format("text was '{}'", outVariables.at("text").toString(false)));
            if (outVariables.at("raw").toString(false) != "\x89PNG")
                throw std::runtime_error(fmt::format("raw was '{}'", outVariables.at("raw").toString(false)));

            return true;
        }
    };

    /**
     * @brief An out variable whose value cannot be read is left out, and does not end the process
     */
    class TestPatternOutVariableError : public TestPattern {
    public:
        TestPatternOutVariableError(core::Evaluator *evaluator) : TestPattern(evaluator, "OutVariableError") { }
        ~TestPatternOutVariableError() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn fail(auto value) {
                    builtin::std::error("no value");
                };

                using Broken = u8 [[transform("fail")]];

                Broken broken out;
                u8 good out;
                good = 1;
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &) const override {
            const auto outVariables = m_runtime->getOutVariables();

            return !outVariables.contains("broken") && outVariables.contains("good");
        }
    };

}
