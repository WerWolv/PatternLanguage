#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_enum.hpp>

namespace pl::ptrn {

    class PatternBitfieldMember : public Pattern {
    public:
        using Pattern::Pattern;

        [[nodiscard]] const PatternBitfieldMember& getTopmostBitfield() const {
            const PatternBitfieldMember* topBitfield = this;
            while (auto parent = topBitfield->getParent()) {
                auto parentBitfield = dynamic_cast<const PatternBitfieldMember*>(parent);
                if (parentBitfield == nullptr)
                    break;

                topBitfield = parentBitfield;
            }

            return *topBitfield;
        }

        [[nodiscard]] virtual u8 getBitOffset() const = 0;

        virtual void setBitOffset(u8 bitOffset) = 0;

        [[nodiscard]] virtual u64 getBitSize() const = 0;

        [[nodiscard]] u128 getTotalBitOffset(u64 fromByteOffset = 0) const {
            return ((this->getOffset() - fromByteOffset) << 3) + this->getBitOffset();
        }

        [[nodiscard]] u128 getBitOffsetForDisplay() const {
            return (this->getTotalBitOffset() - getTopmostBitfield().getTotalBitOffset()) % 8;
        }

        [[nodiscard]] virtual bool isPadding() const { return false; }

        [[nodiscard]] u128 getOffsetForSorting() const override {
            return this->getTotalBitOffset();
        }

        [[nodiscard]] u128 getSizeForSorting() const override {
            return this->getBitSize();
        }

        std::vector<u8> getRawBytes() override {
            return { };
        }
    };

    class PatternBitfieldField : public PatternBitfieldMember {
    public:
        PatternBitfieldField(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, PatternBitfieldMember *parentBitfield = nullptr)
                : PatternBitfieldMember(evaluator, offset, (bitOffset + bitSize + 7) / 8, line), m_bitOffset(bitOffset % 8), m_bitSize(bitSize) {
            if (parentBitfield != nullptr)
                this->setParent(parentBitfield->reference());
        }

        PatternBitfieldField(const PatternBitfieldField &other) : PatternBitfieldMember(other) {
            this->m_padding = other.m_padding;
            this->m_bitOffset = other.m_bitOffset;
            this->m_bitSize = other.m_bitSize;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldField(*this));
        }

        [[nodiscard]] u128 readValue() const {
            return this->getEvaluator()->readBits(this->getOffset(), this->getBitOffset(), this->getBitSize(), this->getSection(), this->getEndian());
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_bitSize == 1 ? "bit" : "bits";
        }

        [[nodiscard]] u8 getBitOffset() const override {
            return this->m_bitOffset;
        }

        void setBitOffset(u8 bitOffset) override {
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

        [[nodiscard]] bool isPadding() const override { return this->m_padding; }
        void setPadding(bool padding) { this->m_padding = padding; }

        void setValue(const core::Token::Literal &value) override {
            std::vector<u8> result;

            const auto &formatterFunctionName = this->getWriteFormatterFunction();
            if (formatterFunctionName.empty()) {
                result = this->getBytesOf(value);
            } else {
                try {
                    const auto function = this->getEvaluator()->findFunction(formatterFunctionName);
                    if (function.has_value()) {
                        auto formatterResult = function->func(this->getEvaluator(), { value });

                        if (formatterResult.has_value()) {
                            result = this->getBytesOf(*formatterResult);
                        }
                    }
                } catch (core::err::EvaluatorError::Exception &error) {
                    wolv::util::unused(error);
                }
            }

            if (!result.empty() && result.size() <= sizeof(u128)) {
                u128 writeValue = 0;
                std::memcpy(&writeValue, result.data(), result.size());

                this->getEvaluator()->writeBits(this->getOffset(), this->getBitOffset(), this->getBitSize(), this->getSection(), this->getEndian(), writeValue);

                this->clearFormatCache();
                this->getParent()->clearFormatCache();
            }
        }

        [[nodiscard]] core::Token::Literal getRawValue() const override {
            return this->readValue();
        }

    protected:
        std::string defaultToString() const override {
            return fmt::format("{}", this->getValue().toUnsigned());
        }

    private:
        u8 m_bitOffset;
        u8 m_bitSize;

        bool m_padding = false;
    };

    class PatternBitfieldFieldSigned : public PatternBitfieldField {
    public:
        using PatternBitfieldField::PatternBitfieldField;

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldFieldSigned(*this));
        }

        [[nodiscard]] core::Token::Literal getRawValue() const override {
            return hlp::signExtend(this->getBitSize(), this->readValue());
        }

    protected:
        std::string defaultToString() const override {
            return fmt::format("{}", this->getValue().toSigned());
        }
    };

    class PatternBitfieldFieldBoolean : public PatternBitfieldField {
    public:
        using PatternBitfieldField::PatternBitfieldField;

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldFieldBoolean(*this));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

    protected:
        std::string defaultToString() const override {
            return fmt::format("{}", this->getValue().toBoolean() ? "true" : "false");
        }

        std::string defaultToDisplayValue() const override {
            const auto value = this->getValue().toUnsigned();
            if (value == 0) {
                return "false";
            }
            if (value == 1) {
                return "true";
            }
            return "true*";
        }
    };

    class PatternBitfieldFieldEnum : public PatternBitfieldField {
    public:
        using PatternBitfieldField::PatternBitfieldField;

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + Pattern::getTypeName();
        }

        void setEnumValues(const std::map<std::string, PatternEnum::EnumValue> &enumValues) {
            this->m_enumValues = enumValues;
        }

        const auto& getEnumValues() const {
            return this->m_enumValues;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!compareCommonProperties<decltype(*this)>(other))
                return false;

            auto &otherEnum = *static_cast<const PatternBitfieldFieldEnum *>(&other);
            if (this->m_enumValues != otherEnum.m_enumValues)
                return false;

            return true;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldFieldEnum(*this));
        }

    protected:
        std::string defaultToString() const override {
            return PatternEnum::getEnumName(this->getTypeName(), this->getValue().toUnsigned(), this->getEnumValues(), this->getBitSize());
        }

    private:
        std::map<std::string, PatternEnum::EnumValue> m_enumValues;
    };

    class PatternBitfieldArray : public PatternBitfieldMember,
                                 public IInlinable,
                                 public IIndexable {
    public:
        PatternBitfieldArray(core::Evaluator *evaluator, u64 offset, u8 firstBitOffset, u128 totalBitSize, u32 line)
                : PatternBitfieldMember(evaluator, offset, size_t((totalBitSize + 7) / 8), line), m_firstBitOffset(firstBitOffset), m_totalBitSize(totalBitSize) { }

        PatternBitfieldArray(const PatternBitfieldArray &other) : PatternBitfieldMember(other) {
            std::vector<std::shared_ptr<Pattern>> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(std::move(entries));

            this->m_firstBitOffset = other.m_firstBitOffset;
            this->m_totalBitSize = other.m_totalBitSize;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            auto other =  std::make_shared<PatternBitfieldArray>(*this);
            for (const auto &entry : other->m_entries)
                entry->setParent(other->reference());
            return other;
        }

        [[nodiscard]] u8 getBitOffset() const override {
            return m_firstBitOffset;
        }

        void setBitOffset(u8 bitOffset) override {
            this->m_firstBitOffset = bitOffset;
        }

        void setBitSize(u128 bitSize) {
            this->m_totalBitSize = bitSize;
            this->setSize(size_t((bitSize + 7) / 8));
        }

        [[nodiscard]] u64 getBitSize() const override {
            return u64(this->m_totalBitSize);
        }

        [[nodiscard]] bool isReversed() const {
            return this->m_reversed;
        }

        void setReversed(bool reversed) {
            this->m_reversed = reversed;
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

        void setSection(u64 id) override {
            if (this->getSection() == id)
                return;

            for (auto &entry : this->m_entries)
                entry->setSection(id);

            PatternBitfieldMember::setSection(id);
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

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getSortedEntries() override {
            if (this->m_sortedEntries.empty())
                return this->getEntries();
            return this->m_sortedEntries;
        }

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries) {
                if (entry->getSection() == this->getSection() && entry->getSection() != ptrn::Pattern::PatternLocalSectionId)
                    entry->setOffset(entry->getOffset() - this->getOffset() + offset);
            }

            PatternBitfieldMember::setOffset(offset);
        }

        void forEachEntryImpl(const std::vector<std::shared_ptr<Pattern>> &patterns, u64 start, u64 end, const std::function<void(u64, const std::shared_ptr<Pattern>&)>& fn) override {
            auto evaluator = this->getEvaluator();
            auto startArrayIndex = evaluator->getCurrentArrayIndex();

            ON_SCOPE_EXIT {
                              if (startArrayIndex.has_value())
                                  evaluator->setCurrentArrayIndex(*startArrayIndex);
                              else
                                  evaluator->clearCurrentArrayIndex();
                          };

            for (u64 i = start; i < std::min<u64>(end, patterns.size()); i++) {
                evaluator->setCurrentArrayIndex(i);

                auto &entry = patterns[i];
                if (this->hasAttribute("export") || !entry->isPatternLocal() || entry->hasAttribute("export"))
                    fn(i, entry);
            }
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) override {
            this->m_entries = entries;
            this->m_sortedEntries.clear();

            for (auto &entry : this->m_entries) {
                if (!entry->hasOverriddenColor())
                    entry->setBaseColor(this->getColor());
            }

            if (!this->m_entries.empty())
                this->setBaseColor(this->m_entries.front()->getColor());
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

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedEntries.clear();
            for (auto &member : this->m_entries)
                this->m_sortedEntries.push_back(member);

            std::ranges::sort(this->m_sortedEntries, [&](const auto & a, const auto & b) {
                return comparator(a.get(), b.get());
            });

            if (this->isReversed())
                std::ranges::reverse(this->m_sortedEntries);

            for (auto &member : this->m_entries)
                member->sort(comparator);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::ranges::reverse(result);

            return result;
        }

        void clearFormatCache() override {
            this->forEachEntry(0, this->getEntryCount(), [&](u64, const auto &entry) {
                entry->clearFormatCache();
            });

            Pattern::clearFormatCache();
        }
    protected:
        std::string defaultToString() const override {
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
            return result;
        }

        std::string defaultToDisplayValue() const override {
            return "[ ... ]";
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_entries;
        std::vector<std::shared_ptr<Pattern>> m_sortedEntries;
        u8 m_firstBitOffset = 0;
        u128 m_totalBitSize = 0;
        bool m_reversed = false;
    };

    class PatternBitfield : public PatternBitfieldMember,
                            public IInlinable,
                            public IIterable {
    public:
        PatternBitfield(core::Evaluator *evaluator, u64 offset, u8 firstBitOffset, u128 totalBitSize, u32 line)
                : PatternBitfieldMember(evaluator, offset, size_t((totalBitSize + 7) / 8), line), m_firstBitOffset(firstBitOffset), m_totalBitSize(totalBitSize) { }

        PatternBitfield(const PatternBitfield &other) : PatternBitfieldMember(other) {
            for (const auto &field : other.m_fields) {
                auto copy = field->clone();
                this->m_fields.push_back(copy);
                this->m_sortedFields.push_back(std::move(copy));
            }

            this->m_firstBitOffset = other.m_firstBitOffset;
            this->m_totalBitSize = other.m_totalBitSize;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfield(*this));
        }

        [[nodiscard]] u8 getBitOffset() const override {
            return m_firstBitOffset;
        }

        void setBitOffset(u8 bitOffset) override {
            this->m_firstBitOffset = bitOffset;
        }

        void setBitSize(u128 bitSize) {
            this->m_totalBitSize = u64(bitSize);
            this->setSize(size_t((bitSize + 7) / 8));
        }

        [[nodiscard]] u64 getBitSize() const override {
            return this->m_totalBitSize;
        }

        [[nodiscard]] bool isReversed() const {
            return this->m_reversed;
        }

        void setReversed(bool reversed) {
            this->m_reversed = reversed;
        }

        void setSection(u64 id) override {
            if (this->getSection() == id)
                return;

            for (auto &field : this->m_fields)
                field->setSection(id);

            Pattern::setSection(id);
        }

        [[nodiscard]] std::vector<std::pair<u64, Pattern*>> getChildren() override {
            if (this->getVisibility() == Visibility::HighlightHidden)
                return { };

            if (this->isSealed()) {
                return { { this->getOffset(), this } };
            } else {
                std::vector<std::pair<u64, Pattern*>> result;

                for (const auto &entry : this->m_fields) {
                    auto children = entry->getChildren();
                    std::move(children.begin(), children.end(), std::back_inserter(result));
                }

                return result;
            }
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &entry : this->m_fields) {
                if (!entry->hasOverriddenColor())
                    entry->setColor(color);
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

        void setFields(std::vector<std::shared_ptr<Pattern>> fields) {
            this->m_fields = std::move(fields);

            if (!this->m_fields.empty())
                this->setBaseColor(this->m_fields.front()->getColor());

            this->m_sortedFields.clear();
            for (const auto &field : this->m_fields) {
                field->setParent(this->reference());
                this->m_sortedFields.push_back(field);
            }
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

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return this->m_fields;
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getSortedEntries() override {
            if (this->m_sortedFields.empty())
                return this->getEntries();
            return this->m_sortedFields;
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) override {
            this->m_fields = entries;
            this->m_sortedFields.clear();
        }

        void setOffset(u64 offset) override {
            for (auto &field : this->m_fields) {
                if (field->getSection() == this->getSection()) {
                    if (field->getSection() == ptrn::Pattern::PatternLocalSectionId)
                        field->setOffset((offset & 0xFFFF'FFFF'0000'0000) | (field->getOffset() & 0xFFFF'FFFF));
                    else
                        field->setOffset(field->getOffset() - this->getOffset() + offset);
                }
            }

            PatternBitfieldMember::setOffset(offset);
        }

        void forEachEntryImpl(const std::vector<std::shared_ptr<Pattern>> &patterns, u64 start, u64 end, const std::function<void (u64, const std::shared_ptr<Pattern>&)> &fn) override {
            if (this->isSealed())
                return;

            end = std::min<u64>(end, patterns.size());
            for (auto i = start; i < end; i++) {
                auto &pattern = patterns[i];
                if (this->hasAttribute("export") || !pattern->isPatternLocal() || pattern->hasAttribute("export"))
                    fn(i, pattern);
            }
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedFields.clear();
            for (auto &member : this->m_fields)
                this->m_sortedFields.push_back(member);

            std::ranges::sort(this->m_sortedFields, [&](const auto & a, const auto & b) {
                return comparator(a.get(), b.get());
            });
            if (this->isReversed())
                std::ranges::reverse(this->m_sortedFields);

            for (auto &member : this->m_fields)
                member->sort(comparator);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::ranges::reverse(result);

            return result;
        }

        void clearFormatCache() override {
            this->forEachEntry(0, this->getEntryCount(), [&](u64, const auto &entry) {
                entry->clearFormatCache();
            });

            Pattern::clearFormatCache();
        }

    protected:
        std::string defaultToString() const override {
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
            return result;
        }

        std::string defaultToDisplayValue() const override {
            std::vector<u8> bytes(this->getSize(), 0);
            this->getEvaluator()->readData(this->getOffset(), bytes.data(), bytes.size(), this->getSection());

            if (this->getEndian() == std::endian::little)
                std::reverse(bytes.begin(), bytes.end());

            std::string valueString;

            for (const auto &pattern : this->m_fields) {
                if (auto *field = dynamic_cast<PatternBitfieldField *>(pattern.get()); field != nullptr) {
                    auto fieldValue = field->getValue().toUnsigned();

                    if (field->getBitSize() == 1) {
                        if (fieldValue > 0)
                            valueString += fmt::format("{} | ", field->getVariableName());
                    } else {
                        valueString += fmt::format("{}({}) | ", field->getVariableName(), field->toString());
                    }
                } else if (auto *member = dynamic_cast<PatternBitfieldMember *>(pattern.get()); member != nullptr) {
                    valueString += fmt::format("{} = {} | ", member->getVariableName(), member->toString());
                } else if (auto *bitfield = dynamic_cast<PatternBitfield *>(pattern.get()); bitfield != nullptr) {
                    auto value = bitfield->callUserFormatFunc().value_or(bitfield->defaultToDisplayValue());
                    valueString += fmt::format("{} = {} | ", bitfield->getVariableName(), std::move(value));
                }
            }

            if (valueString.size() >= 3) {
                valueString.resize(valueString.size() - 3);
            }

            if (valueString.size() > 64)
                return fmt::format("{{ ... }}", valueString);
            else
                return fmt::format("{{ {} }}", valueString);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_fields;
        std::vector<std::shared_ptr<Pattern>> m_sortedFields;

        u8 m_firstBitOffset = 0;
        u64 m_totalBitSize = 0;
        bool m_reversed = false;
    };

}
