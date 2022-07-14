#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternPadding : public Pattern {
    public:
        PatternPadding(Evaluator *evaluator, u64 offset, size_t size) : Pattern(evaluator, offset, size, 0xFF000000) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternPadding(*this));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "";
        }

        [[nodiscard]] virtual std::vector<std::pair<u64, Pattern*>> getChildren() {
            return { };
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            return "";
        }

        [[nodiscard]] std::string toString() const override {
            return fmt::format("padding[{}]", this->getSize());
        }
    };

}