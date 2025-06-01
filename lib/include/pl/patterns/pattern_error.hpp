#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternError : public Pattern {
    protected:
        void initialise(core::Evaluator *evaluator, u64 offset, size_t size, u32 line, std::string errorMessage) {
            (void)errorMessage;
            Pattern::initialise(evaluator, offset, size, line);
        }

        void initialise(const PatternError &other) {
            Pattern::initialise(other);
        }

        PatternError(core::Evaluator *evaluator, u64 offset, size_t size, u32 line, std::string errorMessage)
            : Pattern(evaluator, offset, size, line), m_errorMessage(std::move(errorMessage)) { }

    public:
        static std::shared_ptr<PatternError> create(core::Evaluator *evaluator, u64 offset, size_t size, u32 line, std::string errorMessage) {
            auto p = std::shared_ptr<PatternError>(new PatternError(evaluator, offset, size, line, errorMessage));
            p->initialise(evaluator, offset, size, line, errorMessage);
            return p;
        }

        static std::shared_ptr<PatternError> create(const PatternError &other) {
            auto p = std::shared_ptr<PatternError>(new PatternError(other));
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
            return m_errorMessage;
        }

        [[nodiscard]] std::string toString() override {
            return m_errorMessage;
        }

        std::vector<u8> getRawBytes() override {
            return { };
        }

    private:
        std::string m_errorMessage;
    };

}