#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_struct.hpp>

#include <utility>

namespace pl::test {

    class TestPatternHeapLifetime : public TestPattern {
    public:
        TestPatternHeapLifetime(core::Evaluator *evaluator) : TestPattern(evaluator, "HeapLifetime") {
        }
        ~TestPatternHeapLifetime() override = default;

        void setup() override {
            m_capturedPattern.reset();

            m_runtime->addFunction({ "test" }, "capture_child", api::FunctionParameterCount::exactly(1), [this](core::Evaluator *, const auto &params) -> std::optional<core::Token::Literal> {
                auto pattern = params[0].toPattern();
                auto iterable = dynamic_cast<ptrn::IIterable *>(pattern.get());
                if (iterable == nullptr || iterable->getEntryCount() == 0)
                    return std::nullopt;

                m_capturedPattern = iterable->getEntry(0);

                return std::nullopt;
            });
        }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Pair {
                    u8 a;
                    u8 b;
                };

                fn keep_child() {
                    Pair pair;
                    test::capture_child(pair);
                };

                fn main() {
                    keep_child();
                };
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &patterns) const override {
            wolv::util::unused(patterns);

            if (m_capturedPattern == nullptr)
                return false;

            auto capturedPattern = std::exchange(m_capturedPattern, nullptr);
            try {
                return capturedPattern->getRawBytes().size() == sizeof(u8);
            } catch (...) {
                return false;
            }
        }

    private:
        mutable std::shared_ptr<ptrn::Pattern> m_capturedPattern;
    };

}
