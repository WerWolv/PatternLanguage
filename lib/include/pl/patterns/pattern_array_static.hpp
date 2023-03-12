#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternArrayStatic : public Pattern,
                               public Inlinable,
                               public Iteratable {
    public:
        PatternArrayStatic(core::Evaluator *evaluator, u64 offset, size_t size)
            : Pattern(evaluator, offset, size) { }

        PatternArrayStatic(const PatternArrayStatic &other) : Pattern(other) {
            this->setEntries(other.getTemplate()->clone(), other.getEntryCount());
            this->m_formatCache = other.m_formatCache;
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternArrayStatic(*this));
        }

        [[nodiscard]] std::shared_ptr<Pattern> getEntry(size_t index) const override {
            std::shared_ptr<Pattern> highlightTemplate = this->m_template->clone();
            highlightTemplate->setOffset(this->getOffset() + index * highlightTemplate->getSize());

            return highlightTemplate;
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return { this->getTemplate()->clone() };
        }

        void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)>& fn) override {
            auto evaluator = this->getEvaluator();
            auto startArrayIndex = evaluator->getCurrentArrayIndex();
            ON_SCOPE_EXIT {
                if (startArrayIndex.has_value())
                    evaluator->setCurrentArrayIndex(*startArrayIndex);
                else
                    evaluator->clearCurrentArrayIndex();
            };

            auto &entry = this->m_template;
            for (u64 index = start; index < std::min<u64>(end, this->m_entryCount); index++) {
                entry->setVariableName(fmt::format("[{0}]", index));
                entry->setOffset(this->getOffset() + index * this->m_template->getSize());
                evaluator->setCurrentArrayIndex(index);

                if (this->m_formatCache.contains(index))
                    entry->setFormatValue(this->m_formatCache[index]);
                else
                    entry->clearFormatCache();

                fn(index, entry.get());

                this->m_formatCache[index] = entry->getFormattedValue();
            }
        }

        void setOffset(u64 offset) override {
            this->m_template->setOffset(this->m_template->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        void setSection(u64 id) override {
            this->m_template->setSection(id);

            for (auto &highlightTemplate : this->m_highlightTemplates)
                highlightTemplate->setSection(id);

            Pattern::setSection(id);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            if (this->isSealed())
                return { { this->getOffset(), this } };
            else {
                std::vector<std::pair<u64, Pattern*>> result;

                std::shared_ptr<Pattern> highlightTemplate = this->m_template->clone();

                highlightTemplate->setVariableName(this->getVariableName());
                highlightTemplate->setOffset(this->getOffset());

                this->m_highlightTemplates.push_back(highlightTemplate);

                auto children = highlightTemplate->getChildren();

                result.reserve(this->getEntryCount() * children.size());

                auto templateSize = this->m_template->getSize();
                for (size_t i = 0; i < this->getEntryCount(); i++) {
                    for (const auto &[offset, child] : children) {
                        result.emplace_back(offset + i * templateSize, child);
                    }
                }

                return result;
            }
        }

        void setLocal(bool local) override {
            if (this->m_template != nullptr)
                this->m_template->setLocal(local);

            for (auto &highlightTemplate : this->m_highlightTemplates)
                highlightTemplate->setLocal(local);

            Pattern::setLocal(local);
        }

        void setReference(bool reference) override {
            if (this->m_template != nullptr)
                this->m_template->setReference(reference);

            for (auto &highlightTemplate : this->m_highlightTemplates)
                highlightTemplate->setReference(reference);

            Pattern::setReference(reference);
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            this->m_template->setColor(color);

            for (auto &highlightTemplate : this->m_highlightTemplates)
                highlightTemplate->setColor(color);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_template->getTypeName() + "[" + std::to_string(this->m_entryCount) + "]";
        }

        [[nodiscard]] std::string getTypeName() const override {
            if (this->m_template == nullptr)
                return Pattern::getTypeName();
            else
                return this->m_template->getTypeName();
        }

        [[nodiscard]] const std::shared_ptr<Pattern> &getTemplate() const {
            return this->m_template;
        }

        [[nodiscard]] size_t getEntryCount() const override {
            return this->m_entryCount;
        }

        void setEntryCount(size_t count) {
            this->m_entryCount = count;
        }

        void setEntries(std::unique_ptr<Pattern> &&templatePattern, size_t count) {
            this->m_template          = std::move(templatePattern);
            this->m_highlightTemplates.push_back(this->m_template->clone());
            this->m_entryCount        = count;

            this->m_template->setSection(this->getSection());

            this->m_template->setBaseColor(this->getColor());

            for (auto &highlightTemplate : this->m_highlightTemplates)
                highlightTemplate->setBaseColor(this->getColor());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternArrayStatic *>(&other);
            return *this->m_template == *otherArray.m_template && this->m_entryCount == otherArray.m_entryCount;
        }

        void setEndian(std::endian endian) override {
            if (this->isLocal()) return;

            Pattern::setEndian(endian);

            this->m_template->setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            return Pattern::formatDisplayValue("[ ... ]", this);
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

                entry->setOffset(this->getOffset() + index * this->m_template->getSize());
                entry->clearFormatCache();

                result += fmt::format("{}, ", entry->toString());
            }

            if (this->m_entryCount > 0) {
                // Remove trailing ", "
                result.pop_back();
                result.pop_back();
            }

            result += " ]";

            return Pattern::formatDisplayValue(result, this->clone().get());
        }

    private:
        std::shared_ptr<Pattern> m_template = nullptr;
        mutable std::vector<std::shared_ptr<Pattern>> m_highlightTemplates;
        size_t m_entryCount = 0;

        std::map<u64, std::string> m_formatCache;
    };

}
