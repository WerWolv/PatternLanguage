#pragma once

#include <pl/patterns/pattern.hpp>

#include <codecvt>
#include <locale>

namespace pl::ptrn {

    class PatternWideCharacter : public Pattern, public Iteratable {
    public:
        explicit PatternWideCharacter(core::Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 2, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternWideCharacter(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            char16_t character = '\u0000';
            this->getEvaluator()->readData(this->getOffset(), &character, 2, this->getSection());
            return u128(hlp::changeEndianess(character, this->getEndian()));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            char16_t character = core::Token::literalToUnsigned(value);
            character = hlp::changeEndianess(character, this->getEndian());

            auto result = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>("???").to_bytes(character);

            return this->formatDisplayValue(result, value);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            return this->formatDisplayValue(fmt::format("'{0}'", this->toString()), this->getValue());
        }

        std::shared_ptr<Pattern> getEntry(size_t index) const override {
            return std::make_shared<PatternWideCharacter>(this->getEvaluator(), this->getOffset() + index * sizeof(char16_t), this->getColor());
        }

        size_t getEntryCount() const override {
            return this->getSize() / sizeof(char16_t);
        }

        void forEachEntry(u64 start, u64 end, const std::function<void (u64, Pattern *)> &callback) override {
            for (auto i = start; i < end; i++)
                callback(i, this->getEntry(i).get());
        }
    };

}