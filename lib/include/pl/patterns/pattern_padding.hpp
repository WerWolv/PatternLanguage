#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternPadding : public Pattern {
        BEFRIEND_SHARED_OBJECT_CREATOR
    protected:
        PatternPadding(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) : Pattern(evaluator, offset, size, line) { }

    public:
        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return construct_shared_object<PatternPadding>(*this);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "";
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            return { };
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            return "";
        }

        [[nodiscard]] std::string toString() override {
            auto result = [this]{
               if (this->getSize() == 0)
                   return std::string("null");
               else
                   return fmt::format("padding[{}]", this->getSize());
            }();

            return Pattern::callUserFormatFunc(this->getValue(), true).value_or(result);
        }

        std::vector<u8> getRawBytes() override {
            return { };
        }
    };

}