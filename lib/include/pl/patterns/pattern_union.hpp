#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternUnion : public Pattern,
                         public Inlinable,
                         public Iteratable {
    public:
        PatternUnion(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternUnion(const PatternUnion &other) : Pattern(other) {
            for (const auto &member : other.m_members) {
                auto copy = member->clone();

                this->m_sortedMembers.push_back(copy.get());
                this->m_members.push_back(std::move(copy));
            }
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternUnion(*this));
        }

        [[nodiscard]] std::shared_ptr<Pattern> getEntry(size_t index) const override {
            return this->m_members[index];
        }

        void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)>& fn) override {
            if (this->isSealed())
                return;

            for (u64 i = start; i < this->m_members.size() && i < end; i++)
                fn(i, this->m_members[i].get());
        }

        size_t getEntryCount() const override {
            return this->m_members.size();
        }

        void setOffset(u64 offset) override {
            for (auto &member : this->m_members)
                member->setOffset(member->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            if (this->isSealed())
                return { { this->getOffset(), this } };
            else {
                std::vector<std::pair<u64, Pattern*>> result;

                for (const auto &member : this->m_members) {
                    auto children = member->getChildren();
                    std::copy(children.begin(), children.end(), std::back_inserter(result));
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
            return "union " + Pattern::getTypeName();
        }

        [[nodiscard]] const auto &getMembers() const {
            return this->m_members;
        }

        void setMembers(std::vector<std::shared_ptr<Pattern>> &&members) {
            this->m_members.clear();
            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_sortedMembers.push_back(member.get());
                this->m_members.push_back(std::move(member));
            }

            if (!this->m_members.empty())
                this->setBaseColor(this->m_members.front()->getColor());
        }

        [[nodiscard]] std::string toString() const override {
            std::string result = this->getFormattedName();
            result += " { ";

            for (const auto &member : this->m_members)
                result += fmt::format("{} = {}, ", member->getVariableName(), member->toString());

            if (!this->m_members.empty()) {
                // Remove trailing ", "
                result.pop_back();
                result.pop_back();
            }

            result += " }";

            return this->formatDisplayValue(result, this->clone().get());
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedMembers.clear();
            for (auto &member : this->m_members)
                this->m_sortedMembers.push_back(member.get());

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), comparator);

            for (auto &member : this->m_members)
                member->sort(comparator);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherUnion = *static_cast<const PatternUnion *>(&other);
            if (this->m_members.size() != otherUnion.m_members.size())
                return false;

            for (u64 i = 0; i < this->m_members.size(); i++) {
                if (*this->m_members[i] != *otherUnion.m_members[i])
                    return false;
            }

            return true;
        }

        void setEndian(std::endian endian) override {
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenEndian())
                    member->setEndian(endian);
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
        std::vector<std::shared_ptr<Pattern>> m_members;
        std::vector<Pattern *> m_sortedMembers;
    };

}