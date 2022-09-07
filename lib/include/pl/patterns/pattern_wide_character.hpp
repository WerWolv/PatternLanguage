#pragma once

#include <pl/patterns/pattern.hpp>

#include <codecvt>
#include <locale>

namespace pl::ptrn {

    class PatternWideCharacter : public Pattern {
    public:
        explicit PatternWideCharacter(core::Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 2, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternWideCharacter(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            char16_t character = '\u0000';
            this->getEvaluator()->readData(this->getOffset(), &character, 2, this->isLocal());
            return u128(hlp::changeEndianess(character, this->getEndian()));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }

        [[nodiscard]] std::string toString() const override {
            char16_t character = core::Token::literalToUnsigned(this->getValue());
            character = hlp::changeEndianess(character, this->getEndian());

            auto result = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>("???").to_bytes(character);

            return this->formatDisplayValue(result, this->getValue());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            return this->formatDisplayValue(fmt::format("'{0}'", this->toString()), this->getValue());
        }
    };

}