#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternBoolean : public Pattern {
    public:
        explicit PatternBoolean(Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 1, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBoolean(*this));
        }

        u8 getValue() const {
            u8 boolean = false;
            this->getEvaluator()->readData(this->getOffset(), &boolean, 1);
            return boolean;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        std::string getFormattedValue() override {
            switch (this->getValue()) {
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
            return fmt::format("{}", this->getValue() ? "true" : "false");
        }
    };

}