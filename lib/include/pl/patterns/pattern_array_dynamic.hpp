#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternArrayDynamic : public Pattern,
                                public IInlinable,
                                public IIndexable {
    public:
        PatternArrayDynamic(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        PatternArrayDynamic(const PatternArrayDynamic &other) : Pattern(other) {
            std::vector<std::shared_ptr<Pattern>> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(entries);
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            auto other = std::make_shared<PatternArrayDynamic>(*this);
            for (const auto &entry : other->m_entries)
                entry->setParent(other->reference());

            return other;
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &entry : this->m_entries) {
                if (!entry->hasOverriddenColor())
                    entry->setColor(color);
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            if (this->m_entries.empty())
                return "???";

            return this->m_entries.front()->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

        [[nodiscard]] std::string getTypeName() const override {
            if (this->m_entries.empty())
                return "???";

            return this->m_entries.front()->getTypeName();
        }

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries) {
                if (entry->getSection() == this->getSection() && entry->getSection() != ptrn::Pattern::PatternLocalSectionId)
                    entry->setOffset(entry->getOffset() - this->getOffset() + offset);
            }

            Pattern::setOffset(offset);
        }

        void setSection(u64 id) override {
            if (this->getSection() == id)
                return;

            for (auto &entry : this->m_entries)
                entry->setSection(id);

            Pattern::setSection(id);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            if (this->getVisibility() == Visibility::HighlightHidden)
                return { };

            std::vector<std::pair<u64, Pattern*>> result;

            for (const auto &entry : this->m_entries) {
                auto children = entry->getChildren();
                std::move(children.begin(), children.end(), std::back_inserter(result));
            }

            return result;
        }

        void setLocal(bool local) override {
            for (auto &pattern : this->m_entries)
                pattern->setLocal(local);

            Pattern::setLocal(local);
        }

        void setReference(bool reference) override {
            for (auto &pattern : this->m_entries)
                pattern->setReference(reference);

            Pattern::setReference(reference);
        }

        [[nodiscard]] std::shared_ptr<Pattern> getEntry(size_t index) const override {
            return this->m_entries[index];
        }

        [[nodiscard]] size_t getEntryCount() const override {
            return this->m_entries.size();
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return this->m_entries;
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

            for (u64 i = start; i < std::min<u64>(end, this->m_entries.size()); i++) {
                evaluator->setCurrentArrayIndex(i);

                auto &entry = this->m_entries[i];
                if (!entry->isPatternLocal() || entry->hasAttribute("export"))
                    fn(i, entry.get());
            }
        }

        void addEntry(const std::shared_ptr<Pattern> &entry) override {
            if (entry == nullptr) return;

            if (!entry->hasOverriddenColor())
                entry->setBaseColor(this->getColor());

            this->m_entries.emplace_back(entry);
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) override {
            this->m_entries.clear();

            for (const auto &entry : entries) {
                addEntry(entry);
            }

            if (!this->m_entries.empty())
                this->setBaseColor(this->m_entries.front()->getColor());
        }

        [[nodiscard]] std::string toString() override {
            std::string result;

            result += "[ ";

            size_t entryCount = 0;
            for (const auto &entry : this->m_entries) {
                if (entryCount > 50) {
                    result += fmt::format("..., ");
                    break;
                }

                result += fmt::format("{}, ", entry->toString());
                entryCount++;
            }

            if (entryCount > 0) {
                // Remove trailing ", "
                result.pop_back();
                result.pop_back();
            }

            result += " ]";

            return Pattern::callUserFormatFunc(this->reference(), true).value_or(result);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternArrayDynamic *>(&other);
            if (this->m_entries.size() != otherArray.m_entries.size())
                return false;

            for (u64 i = 0; i < this->m_entries.size(); i++) {
                if (*this->m_entries[i] != *otherArray.m_entries[i])
                    return false;
            }

            return true;
        }

        void setEndian(std::endian endian) override {
            if (this->isLocal()) return;

            Pattern::setEndian(endian);

            for (auto &entry : this->m_entries) {
                entry->setEndian(endian);
            }

        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            return Pattern::callUserFormatFunc(this->reference()).value_or("[ ... ]");
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;

            if (this->isSealed()) {
                result.resize(this->getSize());
                this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            } else {
                this->forEachEntry(0, this->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                    auto bytes = entry->getBytes();
                    std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));
                });
            }

            return result;
        }

        void clearFormatCache() override {
            this->forEachEntry(0, this->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                entry->clearFormatCache();
            });

            Pattern::clearFormatCache();
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_entries;
    };

}