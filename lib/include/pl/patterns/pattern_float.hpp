#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternFloat : public Pattern {
    public:
        PatternFloat(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternFloat(*this));
        }

        double getValue() const {
            if (this->getSize() == 4) {
                u32 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 4);
                data = pl::changeEndianess(data, 4, this->getEndian());

                float result = 0;
                std::memcpy(&result, &data, sizeof(float));
                return result;
            } else if (this->getSize() == 8) {
                u64 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 8);
                data = pl::changeEndianess(data, 8, this->getEndian());

                double result = 0;
                std::memcpy(&result, &data, sizeof(double));
                return result;
            } else {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 4:
                    return "float";
                case 8:
                    return "double";
                default:
                    return "Floating point data";
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            if (this->getSize() == 4) {
                auto f32 = static_cast<float>(this->getValue());
                u32 integerResult = 0;
                std::memcpy(&integerResult, &f32, sizeof(float));
                return this->formatDisplayValue(fmt::format("{:e} (0x{:0{}X})", f32, integerResult, this->getSize() * 2), f32);
            } else if (this->getSize() == 8) {
                auto f64 = static_cast<double>(this->getValue());
                u64 integerResult = 0;
                std::memcpy(&integerResult, &f64, sizeof(double));
                return this->formatDisplayValue(fmt::format("{:e} (0x{:0{}X})", f64, integerResult, this->getSize() * 2), f64);
            } else {
                return "Floating Point Data";
            }
        }

        [[nodiscard]] std::string toString() const override {
            return fmt::format("{}", this->getValue());
        }
    };

}