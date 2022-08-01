#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternArrayStatic : public Pattern,
                               public Inlinable {
    public:
        PatternArrayStatic(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternArrayStatic(const PatternArrayStatic &other) : Pattern(other) {
            this->setEntries(other.getTemplate()->clone(), other.getEntryCount());
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternArrayStatic(*this));
        }

        void forEachArrayEntry(u64 end, const std::function<void(u64, Pattern&)>& fn) {
            auto entry = std::shared_ptr(this->m_template->clone());
            for (u64 index = 0; index < std::min<u64>(end, this->m_entryCount); index++) {
                entry->clearFormatCache();
                entry->setVariableName(fmt::format("[{0}]", index));
                entry->setOffset(this->getOffset() + index * this->m_template->getSize());
                fn(index, *entry);
            }
        }

        void setOffset(u64 offset) override {
            this->m_template->setOffset(this->m_template->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            std::vector<std::pair<u64, Pattern*>> result;

            this->m_highlightTemplate->setVariableName(this->getVariableName());

            for (size_t index = 0; index < this->m_entryCount; index++) {
                this->m_highlightTemplate->setOffset(this->getOffset() + index * this->m_highlightTemplate->getSize());
                this->m_highlightTemplate->clearFormatCache();

                auto children = this->m_highlightTemplate->getChildren();
                std::copy(children.begin(), children.end(), std::back_inserter(result));
            }

            return result;
        }

        void setMemoryLocationType(PatternMemoryType type) override {
            if (this->m_template != nullptr)
                this->m_template->setMemoryLocationType(type);

            if (this->m_highlightTemplate != nullptr)
                this->m_highlightTemplate->setMemoryLocationType(type);

            Pattern::setMemoryLocationType(type);
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            this->m_template->setColor(color);
            this->m_highlightTemplate->setColor(color);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_template->getTypeName() + "[" + std::to_string(this->m_entryCount) + "]";
        }

        [[nodiscard]] std::string getTypeName() const override {
            return this->m_template->getTypeName();
        }

        [[nodiscard]] const std::shared_ptr<Pattern> &getTemplate() const {
            return this->m_template;
        }

        [[nodiscard]] size_t getEntryCount() const {
            return this->m_entryCount;
        }

        void setEntryCount(size_t count) {
            this->m_entryCount = count;
        }

        void setEntries(std::unique_ptr<Pattern> &&templatePattern, size_t count) {
            this->m_template          = std::move(templatePattern);
            this->m_highlightTemplate = this->m_template->clone();
            this->m_entryCount        = count;

            this->m_template->setMemoryLocationType(this->getMemoryLocationType());

            this->m_template->setBaseColor(this->getColor());
            this->m_highlightTemplate->setBaseColor(this->getColor());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternArrayStatic *>(&other);
            return *this->m_template == *otherArray.m_template && this->m_entryCount == otherArray.m_entryCount;
        }

        void setEndian(std::endian endian) override {
            this->m_template->setEndian(endian);

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            this->m_template->clearFormatCache();
            this->m_highlightTemplate->clearFormatCache();

            return this->formatDisplayValue("{ ... }", this);
        }

        [[nodiscard]] std::string toString() const override {
            std::string result;

            result += "[ ";

            auto entry = this->m_template->clone();
            for (size_t index = 0; index < this->m_entryCount; index++) {
                if (index > 50) {
                    result += fmt::format("..., ");
                    break;
                }

                this->m_highlightTemplate->setOffset(this->getOffset() + index * this->m_highlightTemplate->getSize());

                result += fmt::format("{}, ", entry->toString());
            }

            // Remove trailing ", "
            result.pop_back();
            result.pop_back();

            result += " ]";

            return result;
        }

    private:
        std::shared_ptr<Pattern> m_template = nullptr;
        std::unique_ptr<Pattern> m_highlightTemplate = nullptr;
        size_t m_entryCount = 0;
    };

}
