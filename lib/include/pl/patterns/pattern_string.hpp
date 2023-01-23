#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternString : public Pattern, public Iteratable {
    public:
        PatternString(core::Evaluator *evaluator, u64 offset, size_t size)
            : Pattern(evaluator, offset, size) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternString(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(this->getValue(this->getSize()));
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return { };
        }

        std::string getValue(size_t size) const {
            if (size == 0)
                return "";

            std::string buffer(size, '\x00');
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size, this->getSection());

            return buffer;
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            if (auto stringValue = std::get_if<std::string>(&value); stringValue != nullptr)
                return { stringValue->begin(), stringValue->end() };
            else
                return { };
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String";
        }

        [[nodiscard]] std::string toString() const override {
            auto value = this->getValue();
            auto result = value.toString(false);

            return this->formatDisplayValue(result, value);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto size = std::min<size_t>(this->getSize(), 0x7F);

            if (size == 0)
                return "\"\"";

            std::string buffer(size, 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size, this->getSection());
            auto displayString = hlp::encodeByteString({ buffer.begin(), buffer.end() });

            return this->formatDisplayValue(fmt::format("\"{0}\" {1}", displayString, size > this->getSize() ? "(truncated)" : ""), buffer);
        }

        std::shared_ptr<Pattern> getEntry(size_t index) const override {
            auto result = std::make_shared<PatternCharacter>(this->getEvaluator(), this->getOffset() + index);
            result->setSection(this->getSection());

            return result;
        }

        size_t getEntryCount() const override {
            return this->getSize();
        }

        void forEachEntry(u64 start, u64 end, const std::function<void (u64, Pattern *)> &callback) override {
            for (auto i = start; i < end; i++)
                callback(i, this->getEntry(i).get());
        }

    };

}