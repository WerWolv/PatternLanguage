#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternFloat : public Pattern {
    public:
        PatternFloat(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternFloat(*this));
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

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        [[nodiscard]] core::Token::Literal getRawValue() const override {
            if (this->getSize() == 4) {
                u32 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 4, this->getSection());
                data = hlp::changeEndianess(data, 4, this->getEndian());

                return static_cast<double>(std::bit_cast<float>(data));
            }
            if (this->getSize() == 8) {
                u64 data = 0;
                this->getEvaluator()->readData(this->getOffset(), &data, 8, this->getSection());
                data = hlp::changeEndianess(data, 8, this->getEndian());

                return std::bit_cast<double>(data);
            }
            return std::numeric_limits<double>::quiet_NaN();
        }

    protected:
        std::string defaultToString() const override {
            if (this->getSize() == 4) {
                return fmt::format("{}", static_cast<float>(this->getValue().toFloatingPoint()));
            } else {
                return fmt::format("{}", static_cast<double>(this->getValue().toFloatingPoint()));
            }
        }

        std::string defaultToDisplayValue() const override {
            if (this->getSize() == 4) {
                return fmt::format("{:g}", static_cast<float>(this->getValue().toFloatingPoint()));
            }
            if (this->getSize() == 8) {
                return fmt::format("{:g}", static_cast<double>(this->getValue().toFloatingPoint()));
            }
            return "Floating Point Data";
        }
    };

}
