#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_signed.hpp>

namespace pl::ptrn {

    class PatternPointer : public Pattern,
                           public Inlinable {
    public:
        PatternPointer(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color), m_pointedAt(nullptr), m_pointerType(nullptr) {
        }

        PatternPointer(const PatternPointer &other) : Pattern(other) {
            this->m_pointedAt = std::shared_ptr(other.m_pointedAt->clone());

            if (other.m_pointerType) {
                this->m_pointerType = other.m_pointerType->clone();
            }
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternPointer(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            i128 data = 0;
            this->getEvaluator()->readData(this->getOffset(), &data, this->getSize(), this->getSection());
            data = hlp::changeEndianess(data, this->getSize(), this->getEndian());

            if (this->m_signed) {
                return hlp::signExtend(this->getSize() * 8, data);
            }

            return data;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            std::string output;
            if (this->getTypeName().empty()) {
                if (this->m_pointedAt && this->m_pointedAt->getSize() > 0) {
                    output.append(this->m_pointedAt->getFormattedName());
                } else {
                    output.append("< ??? >");
                }
            } else {
                output.append(this->getTypeName());
            }

            return output + "* : " + this->m_pointerType->getTypeName();
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            auto children = this->m_pointedAt->getChildren();
            children.emplace_back(this->getOffset(), this);
            return children;
        }

        void setLocal(bool local) override {
            this->m_pointedAt->setLocal(local);

            Pattern::setLocal(local);
        }

        void setReference(bool reference) override {
            this->m_pointedAt->setReference(reference);

            Pattern::setReference(reference);
        }

        void setPointedAtPattern(std::shared_ptr<Pattern> &&pattern) {
            this->m_pointedAt = std::move(pattern);
            this->m_pointedAt->setVariableName(fmt::format("*({})", this->getVariableName()));
            this->m_pointedAt->setOffset(this->m_pointedAtAddress);

            if (this->hasOverriddenColor())
                this->m_pointedAt->setColor(this->getColor());
        }

        void setPointerTypePattern(std::shared_ptr<Pattern> &&pattern) {
            Pattern::setSize(pattern->getSize());
            Pattern::setEndian(pattern->getEndian());
            this->m_pointerType = std::move(pattern);
            this->m_signed = dynamic_cast<const PatternSigned *>(this->m_pointerType.get()) != nullptr;
        }

        const std::shared_ptr<Pattern>& getPointerType() const {
            return this->m_pointerType;
        }

        void setPointedAtAddress(i128 address) {
            this->m_pointedAtAddress = address + this->m_pointerBase;
        }

        [[nodiscard]] i128 getPointedAtAddress() const {
            return this->m_pointedAtAddress;
        }

        [[nodiscard]] const std::shared_ptr<Pattern> &getPointedAtPattern() {
            return this->m_pointedAt;
        }

        [[nodiscard]] bool isSigned() const {
            return this->m_signed;
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            if (this->m_pointedAt != nullptr) {
                this->m_pointedAt->setColor(color);
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (areCommonPropertiesEqual<decltype(*this)>(other)) {
                auto otherPointer = static_cast<const PatternPointer *>(&other);

                return otherPointer->m_pointedAtAddress == this->m_pointedAtAddress &&
                       otherPointer->m_pointerBase == this->m_pointerBase &&
                       otherPointer->m_signed == this->m_signed &&
                       *otherPointer->m_pointerType == *this->m_pointerType &&
                       *otherPointer->m_pointedAt == *this->m_pointedAt;
            }
            return false;
        }

        void rebase(u64 base) {
            this->m_pointedAtAddress = (this->m_pointedAtAddress - this->m_pointerBase) + base;
            this->m_pointerBase = base;

            if (this->m_pointedAt != nullptr) {
                this->m_pointedAt->setOffset(this->m_pointedAtAddress);
            }
        }

        void setEndian(std::endian endian) override {
            if (this->m_pointedAt != nullptr) {
                this->m_pointedAt->setEndian(endian);
            }

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            auto data = core::Token::literalToSigned(this->getValue());
            return this->formatDisplayValue(fmt::format("*(0x{0:X})", data), this->getValue());
        }

        [[nodiscard]] std::string toString() const override {
            auto result = this->m_pointedAt->toString();

            return this->formatDisplayValue(result, this->clone().get());
        }

    private:
        std::shared_ptr<Pattern> m_pointedAt;
        std::shared_ptr<Pattern> m_pointerType;
        i128 m_pointedAtAddress = 0;
        u64 m_pointerBase = 0;
        bool m_signed = false;
    };

}
