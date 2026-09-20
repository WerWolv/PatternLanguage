#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternCharacter : public Pattern {
    public:
        PatternCharacter(core::Evaluator *evaluator, u64 offset, u32 line)
            : Pattern(evaluator, offset, 1, line) { }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::make_unique<PatternCharacter>(*this);
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
            char character = '\x00';
            this->getEvaluator()->readData(this->getOffset(), &character, 1, this->getSection());

            return character;
        }

    protected:
        std::string defaultToString() const override {
            return fmt::format("{}", hlp::encodeByteString({ static_cast<u8>(this->getValue().toCharacter()) }));
        }

        std::string defaultToDisplayValue() const override {
            return fmt::format("'{0}'", this->defaultToString());
        }
    };

}
