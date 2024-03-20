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
        PatternEnum(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

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

            return fmt::format("{} ({})", this->toString(), hlp::to_hex_string(value, this->getSize() * 2));
        }

        static std::string getEnumName(const std::string &typeName, u128 value, const std::vector<EnumValue> &enumValues) {
            std::string result = typeName + "::";

            bool foundValue = false;
            for (const auto &[min, max, name] : enumValues) {
                if (value >= min.toUnsigned() && value <= max.toUnsigned()) {
                    result += name;
                    foundValue = true;
                    break;
                }
            }

            if (!foundValue)
                result += "???";
            return result;
        }

        [[nodiscard]] std::string toString() const override {
            u128 value = this->getValue().toUnsigned();
            return Pattern::formatDisplayValue(getEnumName(this->getTypeName(), value, this->m_enumValues), this->clone(), true);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

    private:
        std::vector<EnumValue> m_enumValues;
    };

}