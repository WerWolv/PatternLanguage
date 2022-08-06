#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternEnum : public Pattern {
    public:
        PatternEnum(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternEnum(*this));
        }

        u64 getValue() const {
            u64 value = 0;
            this->getEvaluator()->readData(this->getOffset(), &value, this->getSize());
            return hlp::changeEndianess(value, this->getSize(), this->getEndian());
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + Pattern::getTypeName();
        }

        [[nodiscard]] std::string getTypeName() const override {
            return Pattern::getTypeName();
        }

        void setEnumValues(const std::vector<std::pair<core::Token::Literal, std::string>> &enumValues) {
            this->m_enumValues = enumValues;
        }

        const auto& getEnumValues() const {
            return this->m_enumValues;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
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

        std::string getFormattedValue() override {
            return this->formatDisplayValue(fmt::format("{} (0x{:0{}X})", this->toString().c_str(), this->getValue(), this->getSize() * 2), this);
        }

        [[nodiscard]] std::string toString() const override {
            u64 value = this->getValue();

            std::string valueString = this->getTypeName() + "::";

            bool foundValue = false;
            for (auto &[entryValueLiteral, entryName] : this->getEnumValues()) {
                auto visitor = hlp::overloaded {
                        [&, name = entryName](auto &entryValue) {
                            if (static_cast<decltype(entryValue)>(value) == entryValue) {
                                valueString += name;
                                foundValue = true;
                                return true;
                            }

                            return false;
                        },
                        [](const std::string &) { return false; },
                        [](Pattern * const) { return false; },
                };

                bool matches = std::visit(visitor, entryValueLiteral);
                if (matches)
                    break;
            }

            if (!foundValue)
                valueString += "???";

            return valueString;
        }

    private:
        std::vector<std::pair<core::Token::Literal, std::string>> m_enumValues;
    };

}