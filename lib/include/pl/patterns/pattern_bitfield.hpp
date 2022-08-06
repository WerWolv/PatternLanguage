#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternBitfieldField : public Pattern {
    public:
        PatternBitfieldField(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, Pattern *bitfieldPattern = nullptr, u32 color = 0)
            : Pattern(evaluator, offset, 0, color), m_bitOffset(bitOffset), m_bitSize(bitSize), m_bitField(bitfieldPattern) {
        }

        PatternBitfieldField(const PatternBitfieldField &other) : Pattern(other) {
            this->m_padding = other.m_padding;
            this->m_bitOffset = other.m_bitOffset;
            this->m_bitSize = other.m_bitSize;
            this->m_bitField = other.m_bitField;
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldField(*this));
        }

        u64 getValue() const {
            std::vector<u8> value(this->m_bitField->getSize(), 0);
            this->getEvaluator()->readData(this->m_bitField->getOffset(), &value[0], value.size());

            if (this->m_bitField->getEndian() != std::endian::native)
                std::reverse(value.begin(), value.end());

            return hlp::extract(this->m_bitOffset + (this->m_bitSize - 1), this->m_bitOffset, value);
        }

        void setBitfield(Pattern *bitField) {
            this->m_bitField = bitField;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bits";
        }

        [[nodiscard]] u8 getBitOffset() const {
            return this->m_bitOffset;
        }

        void setBitOffset(u8 bitOffset) {
            this->m_bitOffset = bitOffset;
        }

        [[nodiscard]] u8 getBitSize() const {
            return this->m_bitSize;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherBitfieldField = *static_cast<const PatternBitfieldField *>(&other);
            return this->m_bitOffset == otherBitfieldField.m_bitOffset && this->m_bitSize == otherBitfieldField.m_bitSize;
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            return this->formatDisplayValue(fmt::format("{0} (0x{1:X})", this->getValue(), this->getValue()), u128(this->getValue()));
        }

        [[nodiscard]] std::string toString() const override {
            return fmt::format("{}", this->getValue());
        }

        [[nodiscard]] bool isPadding() const { return this->m_padding; }
        void setPadding(bool padding) { this->m_padding = padding; }

    private:
        bool m_padding = false;
        u8 m_bitOffset, m_bitSize;
        Pattern *m_bitField = nullptr;
    };

    class PatternBitfield : public Pattern,
                            public Inlinable {
    public:
        PatternBitfield(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternBitfield(const PatternBitfield &other) : Pattern(other) {
            for (auto &field : other.m_fields)
                this->m_fields.push_back(field->clone());
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfield(*this));
        }

        std::vector<u8> getValue() const {
            std::vector<u8> value(this->getSize(), 0);
            this->getEvaluator()->readData(this->getOffset(), &value[0], value.size());

            if (this->getEndian() == std::endian::little)
                std::reverse(value.begin(), value.end());

            return value;
        }

        void forEachMember(const std::function<void(Pattern&)>& fn) {
            if (this->isSealed())
                return;

            for (auto &field : this->m_fields)
                fn(*field);
        }

        void setOffset(u64 offset) override {
            for (auto &field : this->m_fields)
                field->setOffset(field->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            if (this->isSealed()) {
                return { { this->getOffset(), this } };
            } else {
                std::vector<std::pair<u64, Pattern*>> result;

                for (const auto &field : this->m_fields) {
                    result.emplace_back(field->getOffset(), field.get());
                }

                return result;
            }
        }

        void setMemoryLocationType(PatternMemoryType type) override {
            for (auto &pattern : this->m_fields)
                pattern->setMemoryLocationType(type);

            Pattern::setMemoryLocationType(type);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bitfield " + Pattern::getTypeName();
        }

        [[nodiscard]] const auto &getFields() const {
            return this->m_fields;
        }

        void setFields(std::vector<std::shared_ptr<Pattern>> &&fields) {
            this->m_fields = std::move(fields);

            for (auto &field : this->m_fields) {
                field->setSize(this->getSize());
                field->setColor(this->getColor());
            }
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &field : this->m_fields)
                field->setColor(color);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherBitfield = *static_cast<const PatternBitfield *>(&other);
            if (this->m_fields.size() != otherBitfield.m_fields.size())
                return false;

            for (u64 i = 0; i < this->m_fields.size(); i++) {
                if (*this->m_fields[i] != *otherBitfield.m_fields[i])
                    return false;
            }

            return true;
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        [[nodiscard]] std::string toString() const override {
            std::string result = this->getFormattedName();

            result += " { ";

            for (const auto &field : this->m_fields)
                result += fmt::format("{}, ", field->toString());

            // Remove trailing ", "
            result.pop_back();
            result.pop_back();

            result += " }";

            return result;
        }

        std::string getFormattedValue() override {
            std::string valueString = "{ ";
            for (auto i : this->getValue())
                valueString += fmt::format("{0:02X} ", i);
            valueString += "}";

            return this->formatDisplayValue(valueString, this);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_fields;
    };

}