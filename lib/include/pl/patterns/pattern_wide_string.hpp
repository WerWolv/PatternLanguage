#pragma once

#include <pl/patterns/pattern.hpp>

#include <codecvt>
#include <locale>

namespace pl::ptrn {

    class PatternWideString : public Pattern {
    public:
        PatternWideString(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternWideString(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return this->getValue(this->getSize());
        }

        std::string getValue(size_t size) const {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size, this->getSection());

            for (auto &c : buffer)
                c = hlp::changeEndianess(c, 2, this->getEndian());

            auto it = std::remove_if(buffer.begin(), buffer.end(),
                                     [](auto c) { return c == 0x00; });
            buffer.erase(it, buffer.end());

            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>("???").to_bytes(buffer);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String16";
        }

        [[nodiscard]] std::string toString() const override {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), this->getSize(), this->getSection());

            for (auto &c : buffer)
                c = hlp::changeEndianess(c, 2, this->getEndian());

            std::erase_if(buffer, [](auto c) {
                return c == 0x00;
            });

            auto result = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>("???").to_bytes(buffer);

            return this->formatDisplayValue(result, this->getValue());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto size = std::min<size_t>(this->getSize(), 0x100);

            if (size == 0)
                return "\"\"";

            std::string utf8String = this->getValue(size);

            return this->formatDisplayValue(fmt::format("\"{0}\" {1}", utf8String, size > this->getSize() ? "(truncated)" : ""), utf8String);
        }

    private:

    };

}