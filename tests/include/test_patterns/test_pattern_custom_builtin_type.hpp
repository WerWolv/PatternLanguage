#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternCustomBuiltinType : public TestPattern {
    public:
        TestPatternCustomBuiltinType(core::Evaluator *evaluator) : TestPattern(evaluator, "CustomBuiltInType") {
        }
        ~TestPatternCustomBuiltinType() override = default;

        virtual void setup() override{
            const pl::api::Namespace ns = { "custom_type" };
            m_runtime->addType(ns, "custom_type", pl::api::FunctionParameterCount::exactly(1), [](core::Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                auto pattern = std::make_unique<pl::ptrn::PatternUnsigned>(evaluator, 0, sizeof(u8), 0);
                return pattern;
            });
        };

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                custom_type::custom_type<1> test @ 0;
                std::assert(test == 137, "test should be 137");
            )";
        }

        [[nodiscard]] size_t repeatTimes() const override {
            return 2;
        }
    };
}