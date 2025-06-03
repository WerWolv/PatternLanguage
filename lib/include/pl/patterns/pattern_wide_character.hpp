#pragma once

#include <pl/patterns/pattern.hpp>

#include <wolv/utils/string.hpp>

namespace pl::ptrn {

    class PatternWideCharacter : public Pattern {
    public:
        explicit PatternWideCharacter(core::Evaluator *evaluator, u64 offset, u32 line)
            : Pattern(evaluator, offset, 2, line) { }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return construct_shared_object<PatternWideCharacter>(*this); 
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            char16_t character = u'\u0000';
            this->getEvaluator()->readData(this->getOffset(), &character, 2, this->getSection());
            return transformValue(u128(hlp::changeEndianess(character, this->getEndian())));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            char16_t character = char16_t(value.toUnsigned());
            character = hlp::changeEndianess(character, this->getEndian());

            auto result = wolv::util::utf16ToUtf8(std::u16string(&character, 1)).value_or("???");

            return Pattern::callUserFormatFunc(value, true).value_or(result);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            return Pattern::callUserFormatFunc(this->getValue()).value_or(fmt::format("'{0}'", this->toString()));
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }
    };

}
