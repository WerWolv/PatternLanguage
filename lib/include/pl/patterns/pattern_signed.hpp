#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternSigned : public Pattern {
    protected:
        void initialise(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) {
            Pattern::initialise(evaluator, offset, size, line);
        }

        void initialise(const PatternSigned &other) {
            Pattern::initialise(other);
        }

        PatternSigned(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

    public:
        static std::shared_ptr<PatternSigned> create(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) {
            auto p = std::shared_ptr<PatternSigned>(new PatternSigned(evaluator, offset, size, line));
            p->initialise(evaluator, offset, size, line);
            return p;
        }

        static std::shared_ptr<PatternSigned> create(const PatternSigned &other) {
            auto p = std::shared_ptr<PatternSigned>(new PatternSigned(other));
            p->initialise(other);
            return p;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return create(*this);
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            i128 data = 0;
            this->getEvaluator()->readData(this->getOffset(), &data, this->getSize(), this->getSection());
            data = hlp::changeEndianess(data, this->getSize(), this->getEndian());

            return transformValue(hlp::signExtend(this->getSize() * 8, data));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->getTypeName();
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto data = this->getValue().toSigned();

            return Pattern::callUserFormatFunc(this->getValue()).value_or(fmt::format("{:d}", data));
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            auto result = fmt::format("{:d}", value.toSigned());

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