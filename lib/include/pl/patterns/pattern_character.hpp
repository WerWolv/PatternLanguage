#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternCharacter : public Pattern {
    public:
        explicit PatternCharacter(Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 1, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternCharacter(*this));
        }

        char getValue() {
            char character = '\x00';
            this->getEvaluator()->readData(this->getOffset(), &character, 1);
            return character;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char";
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}