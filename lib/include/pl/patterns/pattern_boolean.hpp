#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternBoolean : public Pattern {
    public:
        explicit PatternBoolean(core::Evaluator *evaluator, u64 offset)
            : Pattern(evaluator, offset, 1) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBoolean(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            bool boolean = false;
            this->getEvaluator()->readData(this->getOffset(), &boolean, 1, this->getSection());

            return transformValue(boolean);
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            if (auto boolValue = std::get_if<bool>(&value); boolValue != nullptr)
                return wolv::util::toContainer<std::vector<u8>>(wolv::util::toBytes(*boolValue));
            else
                return { };
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        std::string formatDisplayValue() override {
            switch (this->getValue().toUnsigned()) {
                case 0: return "false";
                case 1: return "true";
                default: return "true*";
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            auto result = fmt::format("{}", value.toBoolean() ? "true" : "false");

            return Pattern::formatDisplayValue(result, value);
        }
    };

}