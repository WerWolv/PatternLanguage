#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternPadding : public Pattern {
    protected:
        void initialise(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) {
            Pattern::initialise(evaluator, offset, size, line);
        }

        void initialise(const PatternPadding &other) {
            Pattern::initialise(other);
        }

        PatternPadding(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }
  
    public:
        static std::shared_ptr<PatternPadding> create(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) {
            auto p = std::shared_ptr<PatternPadding>(new PatternPadding(evaluator, offset, size, line));
            p->initialise(evaluator, offset, size, line);
            return p;
        }

        static std::shared_ptr<PatternPadding> create(const PatternPadding &other) {
            auto p = std::shared_ptr<PatternPadding>(new PatternPadding(other));
            p->initialise(other);
            return p;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return create(*this);
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