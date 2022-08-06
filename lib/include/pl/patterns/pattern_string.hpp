#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternString : public Pattern {
    public:
        PatternString(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternString(*this));
        }

        std::string getValue() const {
            return this->getValue(this->getSize());
        }

        std::string getValue(size_t size) const {
            if (size == 0)
                return "";

            std::vector<u8> buffer(size, 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size);

            auto string = reinterpret_cast<const char *>(buffer.data());
            return { string, std::min(size, std::strlen(string)) };
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String";
        }

        [[nodiscard]] std::string toString() const override {
            return this->getValue();
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto size = std::min<size_t>(this->getSize(), 0x7F);

            if (size == 0)
                return "\"\"";

            std::vector<u8> buffer(size, 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size);
            auto displayString = hlp::encodeByteString(buffer);

            return this->formatDisplayValue(fmt::format("\"{0}\" {1}", displayString, size > this->getSize() ? "(truncated)" : ""), displayString);
        }

    };

}