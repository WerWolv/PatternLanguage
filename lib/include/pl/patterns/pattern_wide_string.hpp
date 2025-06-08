#pragma once

#include <pl/patterns/pattern.hpp>

#include <wolv/utils/string.hpp>

namespace pl::ptrn {

    class PatternWideString : public Pattern,
                              public IIndexable {
    protected:
        PatternWideString(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

    public:
        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return create_shared_object<PatternWideString>(*this); 
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(this->getValue(this->getSize()));
        }

        std::string getValue(size_t size) const {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), size, this->getSection());

            for (auto &c : buffer)
                c = hlp::changeEndianess(c, 2, this->getEndian());

            auto it = std::remove_if(buffer.begin(), buffer.end(),
                                     [](auto c) { return c == 0x00; });
            buffer.erase(it, buffer.end());

            return wolv::util::utf16ToUtf8(buffer).value_or("???");
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String16";
        }

        [[nodiscard]] std::string toString() override {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            this->getEvaluator()->readData(this->getOffset(), buffer.data(), this->getSize(), this->getSection());

            for (auto &c : buffer)
                c = hlp::changeEndianess(c, 2, this->getEndian());

            std::erase_if(buffer, [](auto c) {
                return c == 0x00;
            });

            auto result = wolv::util::utf16ToUtf8(buffer).value_or("???");

            return Pattern::callUserFormatFunc(this->getValue(), true).value_or(result);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto size = std::min<size_t>(this->getSize(), 0x100);

            if (size == 0)
                return "\"\"";

            std::string utf8String = this->getValue(size);

            return Pattern::callUserFormatFunc(utf8String).value_or(fmt::format("\"{0}\" {1}", utf8String, size > this->getSize() ? "(truncated)" : ""));
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return { };
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &) override {

        }

        std::shared_ptr<Pattern> getEntry(size_t index) const override {
            auto result = create_shared_object<PatternWideCharacter>(this->getEvaluator(), this->getOffset() + index * sizeof(char16_t), getLine());
            result->setSection(this->getSection());

            return result;
        }

        size_t getEntryCount() const override {
            return this->getSize() / sizeof(char16_t);
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

        BEFRIEND_CREATE_SHARED_OBJECT(PatternWideString)
    };

}
