#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternArrayDynamic : public Pattern,
                                public Inlinable {
    public:
        PatternArrayDynamic(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternArrayDynamic(const PatternArrayDynamic &other) : Pattern(other) {
            std::vector<std::shared_ptr<Pattern>> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(std::move(entries));
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternArrayDynamic(*this));
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &entry : this->m_entries)
                entry->setColor(color);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_entries[0]->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

        [[nodiscard]] std::string getTypeName() const override {
            return this->m_entries[0]->getTypeName();
        }

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries)
                entry->setOffset(entry->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            std::vector<std::pair<u64, Pattern*>> result;

            for (const auto &entry : this->m_entries) {
                auto children = entry->getChildren();
                std::copy(children.begin(), children.end(), std::back_inserter(result));
            }

            return result;
        }

        void setLocal(bool local) override {
            for (auto &pattern : this->m_entries)
                pattern->setLocal(local);

            Pattern::setLocal(local);
        }

        [[nodiscard]] size_t getEntryCount() const {
            return this->m_entries.size();
        }

        [[nodiscard]] const auto &getEntries() {
            return this->m_entries;
        }

        void forEachArrayEntry(u64 end, const std::function<void(u64, Pattern&)>& fn) {
            auto evaluator = this->getEvaluator();
            auto startArrayIndex = evaluator->getCurrentArrayIndex();
            PL_ON_SCOPE_EXIT {
                if (startArrayIndex.has_value())
                    evaluator->setCurrentArrayIndex(*startArrayIndex);
                else
                    evaluator->clearCurrentArrayIndex();
            };

            for (u64 i = 0; i < std::min<u64>(end, this->m_entries.size()); i++) {
                evaluator->setCurrentArrayIndex(i);
                fn(i, *this->m_entries[i]);
            }
        }

        void setEntries(std::vector<std::shared_ptr<Pattern>> &&entries) {
            this->m_entries = std::move(entries);

            for (auto &entry : this->m_entries) {
                entry->setBaseColor(this->getColor());
            }

            if (!this->m_entries.empty())
                this->setBaseColor(this->m_entries.front()->getColor());
        }

        [[nodiscard]] std::string toString() const override {
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

            // Remove trailing ", "
            result.pop_back();
            result.pop_back();

            result += " ]";

            return result;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
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
            for (auto &entry : this->m_entries) {
                entry->setEndian(endian);
            }

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            return this->formatDisplayValue("{ ... }", this);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_entries;
    };

}