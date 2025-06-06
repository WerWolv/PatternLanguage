#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternFloat : public Pattern {
    protected:
        PatternFloat(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

    public:
        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return construct_shared_object<PatternFloat>(*this); 
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            if (this->getSize() == 4) {
                u32 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 4, this->getSection());
                data = hlp::changeEndianess(data, 4, this->getEndian());

                float result = 0;
                std::memcpy(&result, &data, sizeof(float));
                return transformValue(double(result));
            } else if (this->getSize() == 8) {
                u64 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 8, this->getSection());
                data = hlp::changeEndianess(data, 8, this->getEndian());

                double result = 0;
                std::memcpy(&result, &data, sizeof(double));
                return transformValue(result);
            } else {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            auto doubleValue = value.toFloatingPoint();
            std::vector<u8> result;

            result.resize(this->getSize());

            if (this->getSize() == 4) {
                auto floatValue = static_cast<float>(doubleValue);
                std::memcpy(result.data(), &floatValue, result.size());
            } else if (this->getSize() == 8) {
                std::memcpy(result.data(), &doubleValue, result.size());
            }

            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->getTypeName();
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto value = this->getValue().toFloatingPoint();
            if (this->getSize() == 4) {
                auto f32 = static_cast<float>(value);
                u32 integerResult = 0;
                std::memcpy(&integerResult, &f32, sizeof(float));
                return Pattern::callUserFormatFunc(f32).value_or(fmt::format("{:g}", f32));
            } else if (this->getSize() == 8) {
                auto f64 = static_cast<double>(value);
                u64 integerResult = 0;
                std::memcpy(&integerResult, &f64, sizeof(double));
                return Pattern::callUserFormatFunc(f64).value_or(fmt::format("{:g}", f64));
            } else {
                return "Floating Point Data";
            }
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();

            std::string result;
            if (this->getSize() == 4)
                result = fmt::format("{}", static_cast<float>(value.toFloatingPoint()));
            else
                result = fmt::format("{}", static_cast<double>(value.toFloatingPoint()));

            return Pattern::callUserFormatFunc(value, true).value_or(result);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        BEFRIEND_CONSTRUCT_SHARED_OBJECT(PatternFloat)
    };

}