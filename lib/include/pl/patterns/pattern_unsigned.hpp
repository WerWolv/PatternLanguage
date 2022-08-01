#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternUnsigned : public Pattern {
    public:
        PatternUnsigned(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternUnsigned(*this));
        }

        u128 getValue() const {
            u128 data = 0;
            this->getEvaluator()->readData(this->getOffset(), &data, this->getSize());
            return pl::changeEndianess(data, this->getSize(), this->getEndian());
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return fmt::format("u{}", this->getSize() * 8);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto data = this->getValue();
            return this->formatDisplayValue(fmt::format("{:d} (0x{:0{}X})", data, data, 1 * 2), data);
        }

        [[nodiscard]] std::string toString() const override {
            return fmt::format("{}", this->getValue());
        }
    };

}