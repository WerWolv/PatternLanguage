#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternFloat : public Pattern {
    public:
        PatternFloat(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternFloat(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            if (this->getSize() == 4) {
                u32 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 4, this->getSection());
                data = hlp::changeEndianess(data, 4, this->getEndian());

                float result = 0;
                std::memcpy(&result, &data, sizeof(float));
                return double(result);
            } else if (this->getSize() == 8) {
                u64 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 8, this->getSection());
                data = hlp::changeEndianess(data, 8, this->getEndian());

                double result = 0;
                std::memcpy(&result, &data, sizeof(double));
                return result;
            } else {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->getTypeName();
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto value = core::Token::literalToFloatingPoint(this->getValue());
            if (this->getSize() == 4) {
                auto f32 = static_cast<float>(value);
                u32 integerResult = 0;
                std::memcpy(&integerResult, &f32, sizeof(float));
                return this->formatDisplayValue(fmt::format("{:g} (0x{:0{}X})", f32, integerResult, this->getSize() * 2), f32);
            } else if (this->getSize() == 8) {
                auto f64 = static_cast<double>(value);
                u64 integerResult = 0;
                std::memcpy(&integerResult, &f64, sizeof(double));
                return this->formatDisplayValue(fmt::format("{:g} (0x{:0{}X})", f64, integerResult, this->getSize() * 2), f64);
            } else {
                return "Floating Point Data";
            }
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            auto result = fmt::format("{}", core::Token::literalToFloatingPoint(value));

            return this->formatDisplayValue(result, value);
        }
    };

}