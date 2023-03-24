#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl::ptrn {

    class PatternBitfieldMember : public Pattern {
    public:
        using Pattern::Pattern;

        virtual void setParentBitfield(PatternBitfieldMember *parent) = 0;

        [[nodiscard]] virtual PatternBitfieldMember const* getParentBitfield() const = 0;

        [[nodiscard]] PatternBitfieldMember const& getTopmostBitfield() const {
            pl::ptrn::PatternBitfieldMember const* topBitfield = this;
            while (auto parentBitfield = topBitfield->getParentBitfield())
                topBitfield = parentBitfield;
            return *topBitfield;
        }

        [[nodiscard]] virtual u8 getBitOffset() const = 0;

        virtual void setBitOffset(u8 bitOffset) = 0;

        [[nodiscard]] virtual u64 getBitSize() const = 0;

        [[nodiscard]] u128 getTotalBitOffset(u64 fromByteOffset = 0) const {
            return ((this->getOffset() - fromByteOffset) << 3) + this->getBitOffset();
        }

        [[nodiscard]] u8 getBitOffsetForDisplay() const {
            return this->getTotalBitOffset() - getTopmostBitfield().getTotalBitOffset();
        }

        [[nodiscard]] virtual bool isPadding() const { return false; }

        [[nodiscard]] u64 getOffsetForSorting() const override {
            // If this member has no parent, that means we are sorting in a byte-based context.
            if (this->getParentBitfield() == nullptr)
                return getOffset();

            return this->getTotalBitOffset();
        }

        [[nodiscard]] size_t getSizeForSorting() const override {
            // If this member has no parent, that means we are sorting in a byte-based context.
            if (this->getParentBitfield() == nullptr)
                return this->getSize();

            return this->getBitSize();
        }
    };

    class PatternBitfieldField : public PatternBitfieldMember {
    public:
        PatternBitfieldField(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, PatternBitfieldMember *parentBitfield = nullptr)
            : PatternBitfieldMember(evaluator, offset, (bitOffset + bitSize + 7) / 8), m_bitOffset(bitOffset), m_bitSize(bitSize), m_parentBitfield(parentBitfield) { }

        PatternBitfieldField(const PatternBitfieldField &other) : PatternBitfieldMember(other) {
            this->m_padding = other.m_padding;
            this->m_bitOffset = other.m_bitOffset;
            this->m_bitSize = other.m_bitSize;
            this->m_parentBitfield = other.m_parentBitfield;
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldField(*this));
        }

        [[nodiscard]] u128 readValue() const {
            return this->getEvaluator()->readBits(this->getOffset(), this->getBitOffset(), this->getBitSize(), this->getSection(), this->getEndian());
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(u128(this->readValue()));
        }

        void setParentBitfield(PatternBitfieldMember *parentBitfield) override {
            this->m_parentBitfield = parentBitfield;
        }

        PatternBitfieldMember const*getParentBitfield() const override {
            return this->m_parentBitfield;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_bitSize == 1 ? "bit" : "bits";
        }

        [[nodiscard]] u8 getBitOffset() const override {
            return this->m_bitOffset;
        }

        void setBitOffset(u8 bitOffset) {
            this->m_bitOffset = bitOffset;
        }

        [[nodiscard]] u64 getBitSize() const override {
            return this->m_bitSize;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherBitfieldField = *static_cast<const PatternBitfieldField *>(&other);
            return this->m_bitOffset == otherBitfieldField.m_bitOffset && this->m_bitSize == otherBitfieldField.m_bitSize;
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto literal = this->getValue();
            auto value =this->getValue().toUnsigned();

            return Pattern::formatDisplayValue(fmt::format("{0} (0x{1:X})", value, value), literal);
        }

        [[nodiscard]] std::string toString() const override {
            auto result = fmt::format("{}", this->getValue().toUnsigned());

            return Pattern::formatDisplayValue(result, this->getValue());
        }

        [[nodiscard]] bool isPadding() const override { return this->m_padding; }
        void setPadding(bool padding) { this->m_padding = padding; }

    private:
        bool m_padding = false;
        size_t m_bitOffset;
        u8 m_bitSize;
        PatternBitfieldMember *m_parentBitfield = nullptr;
    };

    class PatternBitfieldArray : public PatternBitfieldMember,
                                 public Inlinable,
                                 public Iteratable {
    public:
        PatternBitfieldArray(core::Evaluator *evaluator, u64 offset, u8 firstBitOffset, u128 totalBitSize)
            : PatternBitfieldMember(evaluator, offset, (totalBitSize + 7) / 8), m_firstBitOffset(firstBitOffset), m_totalBitSize(totalBitSize) { }

        PatternBitfieldArray(const PatternBitfieldArray &other) : PatternBitfieldMember(other) {
            std::vector<std::shared_ptr<Pattern>> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(std::move(entries));

            this->m_firstBitOffset = other.m_firstBitOffset;
            this->m_totalBitSize = other.m_totalBitSize;
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldArray(*this));
        }

        void setParentBitfield(PatternBitfieldMember *parentBitfield) override {
            this->m_parentBitfield = parentBitfield;
        }

        PatternBitfieldMember const *getParentBitfield() const override {
            return this->m_parentBitfield;
        }

        [[nodiscard]] u8 getBitOffset() const override {
            return m_firstBitOffset;
        }

        void setBitOffset(u8 bitOffset) override {
            this->m_firstBitOffset = bitOffset;
        }

        void setBitSize(u128 bitSize) {
            this->m_totalBitSize = bitSize;
            this->setSize((bitSize + 7) / 8);
        }

        [[nodiscard]] u64 getBitSize() const override {
            return this->m_totalBitSize;
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &entry : this->m_entries)
                if (!entry->hasOverriddenColor())
                    entry->setColor(color);
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

        void setSection(u64 id) override {
            for (auto &entry : this->m_entries)
                entry->setSection(id);

            PatternBitfieldMember::setSection(id);
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

            PatternBitfieldMember::setLocal(local);
        }

        void setReference(bool reference) override {
            for (auto &pattern : this->m_entries)
                pattern->setReference(reference);

            PatternBitfieldMember::setReference(reference);
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

            for (u64 i = start; i < std::min<u64>(end, this->m_sortedEntries.size()); i++) {
                evaluator->setCurrentArrayIndex(i);
                if (!this->m_sortedEntries[i]->isPatternLocal())
                    fn(i, this->m_sortedEntries[i]);
            }
        }

        void setEntries(std::vector<std::shared_ptr<Pattern>> &&entries) {
            this->m_entries = std::move(entries);

            for (auto &entry : this->m_entries) {
                if (!entry->hasOverriddenColor())
                    entry->setBaseColor(this->getColor());

                this->m_sortedEntries.push_back(entry.get());
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

            if (entryCount > 0) {
                // Remove trailing ", "
                result.pop_back();
                result.pop_back();
            }

            result += " ]";

            return Pattern::formatDisplayValue(result, this->clone().get());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternBitfieldArray *>(&other);
            if (this->m_firstBitOffset != otherArray.m_firstBitOffset)
                return false;
            if (this->m_totalBitSize != otherArray.m_totalBitSize)
                return false;

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
            return PatternBitfieldMember::formatDisplayValue("[ ... ]", this);
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedEntries.clear();
            for (auto &member : this->m_entries)
                this->m_sortedEntries.push_back(member.get());

            std::sort(this->m_sortedEntries.begin(), this->m_sortedEntries.end(), comparator);

            for (auto &member : this->m_entries)
                member->sort(comparator);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_entries;
        std::vector<Pattern *> m_sortedEntries;
        u8 m_firstBitOffset = 0;
        u128 m_totalBitSize = 0;
        PatternBitfieldMember *m_parentBitfield = nullptr;
    };

    class PatternBitfield : public PatternBitfieldMember,
                            public Inlinable,
                            public Iteratable {
    public:
        PatternBitfield(core::Evaluator *evaluator, u64 offset, u8 firstBitOffset, u128 totalBitSize)
            : PatternBitfieldMember(evaluator, offset, (totalBitSize + 7) / 8), m_firstBitOffset(firstBitOffset), m_totalBitSize(totalBitSize) { }

        PatternBitfield(const PatternBitfield &other) : PatternBitfieldMember(other) {
            for (auto &field : other.m_fields)
                this->m_fields.push_back(field->clone());

            this->m_parentBitfield = other.m_parentBitfield;
            this->m_firstBitOffset = other.m_firstBitOffset;
            this->m_totalBitSize = other.m_totalBitSize;
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfield(*this));
        }

        void setParentBitfield(PatternBitfieldMember *parentBitfield) override {
            this->m_parentBitfield = parentBitfield;
        }

        PatternBitfieldMember const *getParentBitfield() const override {
            return this->m_parentBitfield;
        }

        [[nodiscard]] u8 getBitOffset() const override {
            return m_firstBitOffset;
        }

        void setBitOffset(u8 bitOffset) override {
            this->m_firstBitOffset = bitOffset;
        }

        void setBitSize(u128 bitSize) {
            this->m_totalBitSize = bitSize;
            this->setSize((bitSize + 7) / 8);
        }

        [[nodiscard]] u64 getBitSize() const override {
            return this->m_totalBitSize;
        }

        void setSection(u64 id) override {
            for (auto &field : this->m_fields)
                if (field->getSection() == ptrn::Pattern::MainSectionId)
                    field->setSection(id);

            Pattern::setSection(id);
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

        void setLocal(bool local) override {
            for (auto &pattern : this->m_fields)
                pattern->setLocal(local);

            Pattern::setLocal(local);
        }

        void setReference(bool reference) override {
            for (auto &pattern : this->m_fields)
                pattern->setReference(reference);

            Pattern::setReference(reference);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bitfield " + Pattern::getTypeName();
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return this->m_fields;
        }

        void setFields(std::vector<std::shared_ptr<Pattern>> fields) {
            this->m_fields = std::move(fields);

            if (!this->m_fields.empty())
                this->setBaseColor(this->m_fields.front()->getColor());

            for (const auto &field : this->m_fields)
                this->m_sortedFields.push_back(field.get());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherBitfield = *static_cast<const PatternBitfield *>(&other);
            if (this->m_firstBitOffset != otherBitfield.m_firstBitOffset)
                return false;
            if (this->m_totalBitSize != otherBitfield.m_totalBitSize)
                return false;

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

            for (const auto &field : this->m_fields) {
                if (field->getVariableName().starts_with("$"))
                    continue;

                result += fmt::format("{} = {}, ", field->getVariableName(), field->toString());
            }

            // Remove trailing ", "
            result.pop_back();
            result.pop_back();

            result += " }";

            return Pattern::formatDisplayValue(result, this->clone().get());
        }

        std::string formatDisplayValue() override {
            std::vector<u8> bytes(this->getSize(), 0);
            this->getEvaluator()->readData(this->getOffset(), bytes.data(), bytes.size(), this->getSection());

            if (this->getEndian() == std::endian::little)
                std::reverse(bytes.begin(), bytes.end());

            std::string valueString;

            for (const auto &pattern : this->m_fields) {
                if (auto *field = dynamic_cast<PatternBitfieldField *>(pattern.get()); field != nullptr) {
                    auto fieldValue = field->getValue().toUnsigned();

                    if (fieldValue > 0) {
                        if (field->getBitSize() == 1)
                            valueString += fmt::format("{} | ", field->getVariableName());
                        else
                            valueString += fmt::format("{}({}) | ", field->getVariableName(), field->toString());
                    }
                } else if (auto *bitfield = dynamic_cast<PatternBitfield *>(pattern.get()); bitfield != nullptr) {
                    valueString += fmt::format("{} = {} | ", bitfield->getVariableName(), bitfield->formatDisplayValue());
                }
            }

            if (valueString.size() >= 3) {
                valueString.pop_back();
                valueString.pop_back();
                valueString.pop_back();
            }

            return Pattern::formatDisplayValue(fmt::format("{{ {} }}", valueString), this);
        }

        void setEndian(std::endian endian) override {
            if (this->isLocal()) return;

            Pattern::setEndian(endian);

            for (auto &field : this->m_fields)
                field->setEndian(endian);
        }

        std::shared_ptr<Pattern> getEntry(size_t index) const override {
            return this->m_fields[index];
        }

        size_t getEntryCount() const override {
            return this->m_fields.size();
        }

        void forEachEntry(u64 start, u64 end, const std::function<void (u64, Pattern *)> &fn) override {
            if (this->isSealed())
                return;

            for (auto i = start; i < end; i++) {
                auto pattern = this->m_sortedFields[i];
                if (!pattern->isPatternLocal())
                    fn(i, pattern);
            }
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedFields.clear();
            for (auto &member : this->m_fields)
                this->m_sortedFields.push_back(member.get());

            std::sort(this->m_sortedFields.begin(), this->m_sortedFields.end(), comparator);

            for (auto &member : this->m_fields)
                member->sort(comparator);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_fields;
        std::vector<Pattern *> m_sortedFields;

        PatternBitfieldMember *m_parentBitfield = nullptr;
        u8 m_firstBitOffset = 0;
        u64 m_totalBitSize = 0;
    };

}