#pragma once

#include <pl/patterns/pattern.hpp>

#include <wolv/utils/string.hpp>

namespace pl::ptrn {
    class PatternWideCharacter : public Pattern {
    public:
        explicit PatternWideCharacter(core::Evaluator *evaluator, u64 offset, u32 line)
            : Pattern(evaluator, offset, 2, line) { }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternWideCharacter(*this));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        [[nodiscard]] core::Token::Literal getRawValue() const override {
            char16_t character = u'\u0000';
            this->getEvaluator()->readData(this->getOffset(), &character, 2, this->getSection());
            return static_cast<u128>(hlp::changeEndianess(character, this->getEndian()));
        }

    protected:
        std::string defaultToString() const override {
            char16_t character = static_cast<char16_t>(this->getValue().toUnsigned());
            character = hlp::changeEndianess(character, this->getEndian());

            return wolv::util::utf16ToUtf8(std::u16string(&character, 1)).value_or("???");
        }

        std::string defaultToDisplayValue() const override {
            return fmt::format("'{0}'", this->defaultToString());
        }
    };
}
