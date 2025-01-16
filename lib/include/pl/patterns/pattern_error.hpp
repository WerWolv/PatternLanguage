#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternError : public Pattern {
    public:
        PatternError(core::Evaluator *evaluator, u64 offset, size_t size, u32 line, std::string errorMessage)
            : Pattern(evaluator, offset, size, line), m_errorMessage(std::move(errorMessage)) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternError(*this));
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
            return m_errorMessage;
        }

        [[nodiscard]] std::string toString() const override {
            return m_errorMessage;
        }

        std::vector<u8> getRawBytes() override {
            return { };
        }

    private:
        std::string m_errorMessage;
    };

}