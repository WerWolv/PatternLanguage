#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternBoolean : public Pattern {
    public:
        explicit PatternBoolean(core::Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 1, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBoolean(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            bool boolean = false;
            this->getEvaluator()->readData(this->getOffset(), &boolean, 1, this->getSection());

            return boolean;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        std::string getFormattedValue() override {
            switch (core::Token::literalToUnsigned(this->getValue())) {
                case 0: return "false";
                case 1: return "true";
                default: return "true*";
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            auto result = fmt::format("{}", core::Token::literalToBoolean(value) ? "true" : "false");

            return this->formatDisplayValue(result, value);
        }
    };

}