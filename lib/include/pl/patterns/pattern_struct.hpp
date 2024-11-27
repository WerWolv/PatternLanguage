#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternStruct : public Pattern,
                          public IInlinable,
                          public IIterable {
    public:
        PatternStruct(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        PatternStruct(const PatternStruct &other) : Pattern(other) {
            for (const auto &member : other.m_members) {
                auto copy = member->clone();

                this->m_sortedMembers.push_back(copy.get());
                this->m_members.push_back(std::move(copy));
            }
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternStruct(*this));
        }

        [[nodiscard]] std::shared_ptr<Pattern> getEntry(size_t index) const override {
            return this->m_members[index];
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return this->m_members;
        }

        void addEntry(const std::shared_ptr<Pattern> &entry) override {
            if (entry == nullptr) return;

            entry->setParent(this);
            this->m_sortedMembers.push_back(entry.get());
            this->m_members.push_back(entry);
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) override {
            this->m_members.clear();

            for (const auto &member : entries) {
                addEntry(member);
            }

            if (!this->m_members.empty())
                this->setBaseColor(this->m_members.front()->getColor());
        }

        void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)>& fn) override {
            if (this->isSealed())
                return;

            for (u64 i = start; i < this->m_members.size() && i < end; i++) {
                auto &pattern = this->m_members[i];
                if (!pattern->isPatternLocal() || pattern->hasAttribute("export"))
                    fn(i, pattern.get());
            }
        }

        size_t getEntryCount() const override {
            return this->m_members.size();
        }

        void setOffset(u64 offset) override {
            for (auto &member : this->m_members) {
                if (member->getSection() == this->getSection()) {
                    if (member->getSection() != ptrn::Pattern::PatternLocalSectionId)
                        member->setOffset(member->getOffset() - this->getOffset() + offset);
                    else
                        member->setOffset(offset);
                }
            }

            Pattern::setOffset(offset);
        }

        void setSection(u64 id) override {
            if (this->getSection() == id)
                return;

            for (auto &member : this->m_members)
                member->setSection(id);

            Pattern::setSection(id);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            if (this->getVisibility() == Visibility::HighlightHidden)
                return { };

            if (this->isSealed())
                return { { this->getOffset(), this } };
            else {
                std::vector<std::pair<u64, Pattern*>> result;

                for (const auto &member : this->m_members) {
                    auto children = member->getChildren();
                    std::move(children.begin(), children.end(), std::back_inserter(result));
                }

                return result;
            }
        }

        void setLocal(bool local) override {
            for (auto &pattern : this->m_members)
                pattern->setLocal(local);

            Pattern::setLocal(local);
        }

        void setReference(bool reference) override {
            for (auto &pattern : this->m_members)
                pattern->setReference(reference);

            Pattern::setReference(reference);
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenColor())
                    member->setColor(color);
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "struct " + Pattern::getTypeName();
        }

        [[nodiscard]] std::string toString() const override {
            std::string result = this->getFormattedName();
            result += " { ";

            for (const auto &member : this->m_members) {
                if (member->getVariableName().starts_with("$"))
                    continue;

                result += fmt::format("{} = {}, ", member->getVariableName(), member->toString());
            }

            if (!this->m_members.empty()) {
                // Remove trailing ", "
                result.pop_back();
                result.pop_back();
            }

            result += " }";

            return Pattern::callUserFormatFunc(this->clone(), true).value_or(result);
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedMembers.clear();
            for (auto &member : this->m_members)
                this->m_sortedMembers.push_back(member.get());

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), comparator);

            for (auto &member : this->m_sortedMembers)
                member->sort(comparator);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherStruct = *static_cast<const PatternStruct *>(&other);
            if (this->m_members.size() != otherStruct.m_members.size())
                return false;

            for (u64 i = 0; i < this->m_members.size(); i++) {
                if (*this->m_members[i] != *otherStruct.m_members[i])
                    return false;
            }

            return true;
        }

        void setEndian(std::endian endian) override {
            if (this->isLocal()) return;

            Pattern::setEndian(endian);

            for (auto &member : this->m_members) {
                if (!member->hasOverriddenEndian())
                    member->setEndian(endian);
            }
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            return Pattern::callUserFormatFunc(this->clone()).value_or("{ ... }");
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
        std::vector<std::shared_ptr<Pattern>> m_members;
        std::vector<Pattern *> m_sortedMembers;
    };

}