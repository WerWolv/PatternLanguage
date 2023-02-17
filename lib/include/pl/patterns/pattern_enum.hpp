#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternEnum : public Pattern {
    public:
        struct EnumValue {
            core::Token::Literal min, max;
            std::string name;

            [[nodiscard]] bool operator==(const EnumValue &other) const = default;
            [[nodiscard]] bool operator!=(const EnumValue &other) const = default;
        };

    public:
        PatternEnum(core::Evaluator *evaluator, u64 offset, size_t size)
            : Pattern(evaluator, offset, size) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternEnum(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            u128 value = 0;
            this->getEvaluator()->readData(this->getOffset(), &value, this->getSize(), this->getSection());

            return transformValue(hlp::changeEndianess(value, this->getSize(), this->getEndian()));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + Pattern::getTypeName();
        }

        [[nodiscard]] std::string getTypeName() const override {
            return Pattern::getTypeName();
        }

        void setEnumValues(const std::vector<EnumValue> &enumValues) {
            this->m_enumValues = enumValues;
        }

        const auto& getEnumValues() const {
            return this->m_enumValues;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherEnum = *static_cast<const PatternEnum *>(&other);
            if (this->m_enumValues.size() != otherEnum.m_enumValues.size())
                return false;

            for (u64 i = 0; i < this->m_enumValues.size(); i++) {
                if (this->m_enumValues[i] != otherEnum.m_enumValues[i])
                    return false;
            }

            return true;
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto value = this->getValue().toUnsigned();

            return fmt::format("{} (0x{:0{}X})", this->toString(), value, this->getSize() * 2);
        }

        [[nodiscard]] std::string toString() const override {
            u128 value = this->getValue().toUnsigned();

            std::string result = this->getTypeName() + "::";

            bool foundValue = false;
            for (auto &[min, max, name] : this->getEnumValues()) {
                if (value >= min.toUnsigned() && value <= max.toUnsigned()) {
                    result += name;
                    foundValue = true;
                    break;
                }
            }

            if (!foundValue)
                result += "???";

            return Pattern::formatDisplayValue(result, this->clone().get());
        }

    private:
        std::vector<EnumValue> m_enumValues;
    };

}