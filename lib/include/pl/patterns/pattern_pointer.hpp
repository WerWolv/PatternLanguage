#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternPointer : public Pattern,
                           public Inlinable {
    public:
        PatternPointer(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color), m_pointedAt(nullptr) {
        }

        PatternPointer(const PatternPointer &other) : Pattern(other) {
            this->m_pointedAt = std::shared_ptr(other.m_pointedAt->clone());
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternPointer(*this));
        }

        u64 getValue() {
            u64 data = 0;
            this->getEvaluator()->readData(this->getOffset(), &data, this->getSize());
            return pl::changeEndianess(data, this->getSize(), this->getEndian());
        }

        [[nodiscard]] std::string getFormattedName() const override {
            std::string result = (this->getTypeName().empty() ? this->m_pointedAt->getTypeName() : this->getTypeName()) + "* : ";
            switch (this->getSize()) {
                case 1:
                    result += "u8";
                    break;
                case 2:
                    result += "u16";
                    break;
                case 4:
                    result += "u32";
                    break;
                case 8:
                    result += "u64";
                    break;
                case 16:
                    result += "u128";
                    break;
            }

            return result;
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            auto children = this->m_pointedAt->getChildren();
            children.emplace_back(this->getOffset(), this);
            return children;
        }

        void setMemoryLocationType(PatternMemoryType type) override {
            this->m_pointedAt->setMemoryLocationType(type);

            Pattern::setMemoryLocationType(type);
        }

        void setPointedAtPattern(std::unique_ptr<Pattern> &&pattern) {
            this->m_pointedAt = std::move(pattern);
            this->m_pointedAt->setVariableName(fmt::format("*({})", this->getVariableName()));
            this->m_pointedAt->setOffset(this->m_pointedAtAddress);
            this->m_pointedAt->setColor(Pattern::getColor());
        }

        void setPointedAtAddress(u64 address) {
            this->m_pointedAtAddress = address;
        }

        [[nodiscard]] u64 getPointedAtAddress() const {
            return this->m_pointedAtAddress;
        }

        [[nodiscard]] const std::shared_ptr<Pattern> &getPointedAtPattern() {
            return this->m_pointedAt;
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            if (this->m_pointedAt != nullptr) {
                this->m_pointedAt->setColor(color);
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            return areCommonPropertiesEqual<decltype(*this)>(other) &&
                   *static_cast<const PatternPointer *>(&other)->m_pointedAt == *this->m_pointedAt;
        }

        void rebase(u64 base) {
            if (this->m_pointedAt != nullptr) {
                this->m_pointedAtAddress = (this->m_pointedAt->getOffset() - this->m_pointerBase) + base;
                this->m_pointedAt->setOffset(this->m_pointedAtAddress);
            }

            this->m_pointerBase = base;
        }

        void setEndian(std::endian endian) override {
            this->m_pointedAt->setEndian(endian);

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto data = this->getValue();
            return this->formatDisplayValue(fmt::format("*(0x{0:X})", data), u128(data));
        }

        [[nodiscard]] virtual std::string toString() const {
            return this->m_pointedAt->toString();
        }

    private:
        std::shared_ptr<Pattern> m_pointedAt;
        u64 m_pointedAtAddress = 0;

        u64 m_pointerBase = 0;
    };

}
