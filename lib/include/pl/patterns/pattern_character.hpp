#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternCharacter : public Pattern {
    protected:
        PatternCharacter(core::Evaluator *evaluator, u64 offset, u32 line)
            : Pattern(evaluator, offset, 1, line) { }
    
    public:
        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return create_shared_object<PatternCharacter>(*this);
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            char character = '\x00';
            this->getEvaluator()->readData(this->getOffset(), &character, 1, this->getSection());

            return transformValue(character);
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            if (auto charValue = std::get_if<char>(&value); charValue != nullptr)
                return wolv::util::toContainer<std::vector<u8>>(wolv::util::toBytes(*charValue));
            else
                return { };
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char";
        }

        std::string formatDisplayValue() override {
            const u8 character = this->getValue().toCharacter();
            return Pattern::callUserFormatFunc(this->getValue()).value_or(fmt::format("'{0}'", hlp::encodeByteString({ character })));
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            auto result = fmt::format("{}", hlp::encodeByteString({ u8(value.toCharacter()) }));

            return Pattern::callUserFormatFunc(value, true).value_or(result);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        BEFRIEND_CREATE_SHARED_OBJECT(PatternCharacter)
    };

}