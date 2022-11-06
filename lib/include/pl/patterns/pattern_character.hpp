#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternCharacter : public Pattern {
    public:
        explicit PatternCharacter(core::Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 1, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternCharacter(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            char character = '\x00';
            this->getEvaluator()->readData(this->getOffset(), &character, 1, this->getSection());

            return character;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char";
        }

        std::string getFormattedValue() override {
            const char character = core::Token::literalToCharacter(this->getValue());
            return this->formatDisplayValue(fmt::format("'{0}'", character), this->getValue());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            auto result = fmt::format("{}", hlp::encodeByteString({ u8(core::Token::literalToCharacter(value)) }));

            return this->formatDisplayValue(result, value);
        }
    };

}