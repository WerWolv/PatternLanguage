#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternBoolean : public Pattern {
    public:
        explicit PatternBoolean(core::Evaluator *evaluator, u64 offset, u32 line)
            : Pattern(evaluator, offset, 1, line) { }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return construct_shared_object<PatternBoolean>(*this);
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            bool boolean = false;
            this->getEvaluator()->readData(this->getOffset(), &boolean, 1, this->getSection());

            return transformValue(boolean);
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            if (auto boolValue = std::get_if<bool>(&value); boolValue != nullptr)
                return wolv::util::toContainer<std::vector<u8>>(wolv::util::toBytes(*boolValue));
            else
                return { };
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        std::string formatDisplayValue() override {
            auto value = this->getValue().toUnsigned();
            if (value == 0)
                return "false";
            if (value == 1)
                return "true";
            return "true*";
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            auto result = fmt::format("{}", value.toBoolean() ? "true" : "false");

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
    };

}