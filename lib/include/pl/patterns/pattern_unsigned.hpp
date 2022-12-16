#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternUnsigned : public Pattern {
    public:
        PatternUnsigned(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternUnsigned(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            u128 data = 0;
            this->getEvaluator()->readData(this->getOffset(), &data, this->getSize(), this->getSection());
            return hlp::changeEndianess(data, this->getSize(), this->getEndian());
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            auto unsignedValue = core::Token::literalToUnsigned(value);
            std::vector<u8> result;

            result.resize(this->getSize());
            std::memcpy(result.data(), &unsignedValue, result.size());

            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->getTypeName();
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto data = core::Token::literalToUnsigned(this->getValue());
            return this->formatDisplayValue(fmt::format("{:d}", data), this->getValue());
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            auto result = fmt::format("{}", core::Token::literalToUnsigned(value));

            return this->formatDisplayValue(result, value);
        }
    };

}