#pragma once

#include <pl/patterns/pattern.hpp>

#include <pl/patterns/pattern_character.hpp>

namespace pl::ptrn {

    class PatternString : public Pattern,
                          public IIndexable {
         BEFRIEND_SHARED_OBJECT_CREATOR
    protected:
        PatternString(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

    public:
        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return construct_shared_object<PatternString>(*this);
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(this->getValue(this->getSize()));
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return { };
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &) override {

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

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            auto result = value.toString(false);

            return Pattern::callUserFormatFunc(value, true).value_or(result);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto size = std::min<size_t>(this->getSize(), 0x7F);

            if (size == 0)
                return "\"\"";

            std::string buffer(size, 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size, this->getSection());
            auto displayString = hlp::encodeByteString({ buffer.begin(), buffer.end() });

            return Pattern::callUserFormatFunc(buffer).value_or(fmt::format("\"{0}\" {1}", displayString, size > this->getSize() ? "(truncated)" : ""));
        }

        std::shared_ptr<Pattern> getEntry(size_t index) const override {
            auto result = construct_shared_object<PatternCharacter>(this->getEvaluator(), this->getOffset() + index, getLine());
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

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;

            this->forEachEntry(0, this->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                auto bytes = entry->getBytes();
                std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));
            });

            return result;
        }

    };

}