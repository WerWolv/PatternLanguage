#pragma once

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_enum.hpp>

namespace pl::ptrn {

    class PatternBitfieldMember : public Pattern {
    protected:
        void initialise(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) {
            Pattern::initialise(evaluator, offset, size, line);
        }

        void initialise(const PatternBitfieldMember &other) {
            Pattern::initialise(other);
        }

        PatternBitfieldMember(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        PatternBitfieldMember(const PatternBitfieldMember &other) : Pattern(other) { }

    public:
        /*static std::shared_ptr<PatternBitfieldMember> create(core::Evaluator *evaluator, u64 offset, size_t size, u32 line) {
            auto p = std::shared_ptr<PatternBitfieldMember>(new PatternBitfieldMember(evaluator, offset, size, line));
            p->initialise(evaluator, offset, size, line);
            return p;
        }

        static std::shared_ptr<PatternBitfieldMember> create(const PatternBitfieldMember &other) {
            auto p = std::shared_ptr<PatternBitfieldMember>(new PatternBitfieldMember(other));
            p->initialise(other);
            return p;
        }*/

        [[nodiscard]] const PatternBitfieldMember& getTopmostBitfield() const {
            const PatternBitfieldMember* topBitfield = this;
            while (auto parent = topBitfield->getParent()) {
                auto parentBitfield = dynamic_cast<const PatternBitfieldMember*>(parent.get());
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
    protected:
        void initialise(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, std::shared_ptr<PatternBitfieldMember> parentBitfield = nullptr) {
            PatternBitfieldMember::initialise(evaluator, offset, (bitOffset + bitSize + 7) / 8, line);

            this->setParent(parentBitfield);
        }

        void initialise(const PatternBitfieldField &other) {
            PatternBitfieldMember::initialise(other);

            this->m_padding = other.m_padding;
            this->m_bitOffset = other.m_bitOffset;
            this->m_bitSize = other.m_bitSize;
        }

        PatternBitfieldField(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, std::shared_ptr<PatternBitfieldMember> parentBitfield = nullptr)
            : PatternBitfieldMember(evaluator, offset, (bitOffset + bitSize + 7) / 8, line), m_bitOffset(bitOffset % 8), m_bitSize(bitSize) {
            (void)parentBitfield;
        }

        PatternBitfieldField(const PatternBitfieldField &other) : PatternBitfieldMember(other) { }

    public:
        static std::shared_ptr<PatternBitfieldField> create(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, std::shared_ptr<PatternBitfieldMember> parentBitfield = nullptr) {
            auto p = std::shared_ptr<PatternBitfieldField>(new PatternBitfieldField(evaluator, offset, bitOffset, bitSize, line, parentBitfield));
            p->initialise(evaluator, offset, bitOffset, bitSize, line, parentBitfield);
            return p;
        }

        static std::shared_ptr<PatternBitfieldField> create(const PatternBitfieldField &other) {
            auto p = std::shared_ptr<PatternBitfieldField>(new PatternBitfieldField(other));
            p->initialise(other);
            return p;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return create(*this);
        }

        [[nodiscard]] u128 readValue() const {
            return this->getEvaluator()->readBits(this->getOffset(), this->getBitOffset(), this->getBitSize(), this->getSection(), this->getEndian());
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(this->readValue());
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

        std::string formatDisplayValue() override {
            auto literal = this->getValue();
            auto value = literal.toUnsigned();
            return Pattern::callUserFormatFunc(literal).value_or(fmt::format("{}", value));
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->readValue();
            return Pattern::callUserFormatFunc(value, true).value_or(fmt::format("{}", value));
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

    private:
        u8 m_bitOffset;
        u8 m_bitSize;

        bool m_padding = false;
    };

    class PatternBitfieldFieldSigned : public PatternBitfieldField {
    protected:
           void initialise(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, std::shared_ptr<PatternBitfieldMember> parentBitfield = nullptr) {
            PatternBitfieldField::initialise(evaluator, offset, bitOffset, bitSize, line, parentBitfield);
        }

        void initialise(const PatternBitfieldField &other) {
            PatternBitfieldField::initialise(other);
        }

        PatternBitfieldFieldSigned(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, std::shared_ptr<PatternBitfieldMember> parentBitfield = nullptr)
            : PatternBitfieldField(evaluator, offset, bitOffset, bitSize, line, parentBitfield) {}

        PatternBitfieldFieldSigned(const PatternBitfieldFieldSigned &other) : PatternBitfieldField(other) { }

    public:
        static std::shared_ptr<PatternBitfieldFieldSigned> create(core::Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, u32 line, std::shared_ptr<PatternBitfieldMember> parentBitfield = nullptr) {
            auto p = std::shared_ptr<PatternBitfieldFieldSigned>(new PatternBitfieldFieldSigned(evaluator, offset, bitOffset, bitSize, line, parentBitfield));
            p->initialise(evaluator, offset, bitOffset, bitSize, line, parentBitfield);
            return p;
        }

        static std::shared_ptr<PatternBitfieldFieldSigned> create(const PatternBitfieldFieldSigned &other) {
            auto p = std::shared_ptr<PatternBitfieldFieldSigned>(new PatternBitfieldFieldSigned(other));
            p->initialise(other);
            return p;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return create(*this);
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(hlp::signExtend(this->getBitSize(), this->readValue()));
        }

        std::string formatDisplayValue() override {
            auto rawValue = this->readValue();
            auto value = hlp::signExtend(this->getBitSize(), i128(rawValue));
            return Pattern::callUserFormatFunc(value).value_or(fmt::format("{}", value));
        }

        [[nodiscard]] std::string toString() override {
            auto result = fmt::format("{}", this->getValue().toSigned());
            return Pattern::callUserFormatFunc(this->getValue(), true).value_or(result);
        }
    };

    class PatternBitfieldFieldBoolean : public PatternBitfieldField {
    public:
        using PatternBitfieldField::PatternBitfieldField;

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::shared_ptr<Pattern>(new PatternBitfieldFieldBoolean(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(this->readValue());
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        std::string formatDisplayValue() override {
            auto value = this->getValue().toUnsigned();
            if (value == 0)
                return "false";
            else if (value == 1)
                return "true";
            else
                return "true*";
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            return Pattern::callUserFormatFunc(value, true).value_or(fmt::format("{}", value.toBoolean() ? "true" : "false"));
        }
    };

    class PatternBitfieldFieldEnum : public PatternBitfieldField {
    public:
        using PatternBitfieldField::PatternBitfieldField;

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + Pattern::getTypeName();
        }

        [[nodiscard]] std::string getTypeName() const override {
            return Pattern::getTypeName();
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
            return std::shared_ptr<Pattern>(new PatternBitfieldFieldEnum(*this));
        }

        std::string formatDisplayValue() override {
            auto value = this->readValue();
            auto enumName = PatternEnum::getEnumName(this->getTypeName(), value, this->getEnumValues());
            return Pattern::callUserFormatFunc(value).value_or(fmt::format("{}", enumName));
        }

        [[nodiscard]] std::string toString() override {
            auto enumName = PatternEnum::getEnumName(this->getTypeName(), this->readValue(), this->getEnumValues());
            return Pattern::callUserFormatFunc(this->getValue(), true).value_or(enumName);
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
            return std::shared_ptr<Pattern>(new PatternBitfieldArray(*this));
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

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries) {
                if (entry->getSection() == this->getSection()) {
                    if (entry->getSection() != ptrn::Pattern::PatternLocalSectionId)
                        entry->setOffset(entry->getOffset() - this->getOffset() + offset);
                    else
                        entry->setOffset(offset);
                }
            }

            PatternBitfieldMember::setOffset(offset);
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

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) override {
            this->m_entries = entries;

            for (auto &entry : this->m_entries) {
                if (!entry->hasOverriddenColor())
                    entry->setBaseColor(this->getColor());

                entry->setParent(this->reference());

                this->m_sortedEntries.push_back(entry.get());
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
            return Pattern::callUserFormatFunc(this->reference()).value_or("[ ... ]");
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedEntries.clear();
            for (auto &member : this->m_entries)
                this->m_sortedEntries.push_back(member.get());

            std::sort(this->m_sortedEntries.begin(), this->m_sortedEntries.end(), comparator);
            if (this->isReversed())
                std::reverse(this->m_sortedEntries.begin(), this->m_sortedEntries.end());

            for (auto &member : this->m_entries)
                member->sort(comparator);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

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
        std::vector<Pattern *> m_sortedEntries;
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
            for (auto &field : other.m_fields)
                this->m_fields.push_back(field->clone());

            this->m_firstBitOffset = other.m_firstBitOffset;
            this->m_totalBitSize = other.m_totalBitSize;
        }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::shared_ptr<Pattern>(new PatternBitfield(*this));
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

            for (const auto &field : this->m_fields) {
                field->setParent(this->reference());
                this->m_sortedFields.push_back(field.get());
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

        [[nodiscard]] std::string toString() override {
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

            return Pattern::callUserFormatFunc(this->reference(), true).value_or(result);
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

                    if (field->getBitSize() == 1) {
                        if (fieldValue > 0)
                            valueString += fmt::format("{} | ", field->getVariableName());
                    } else {
                        valueString += fmt::format("{}({}) | ", field->getVariableName(), field->toString());
                    }
                } else if (auto *member = dynamic_cast<PatternBitfieldMember *>(pattern.get()); member != nullptr) {
                    valueString += fmt::format("{} = {} | ", member->getVariableName(), member->toString());
                } else if (auto *bitfield = dynamic_cast<PatternBitfield *>(pattern.get()); bitfield != nullptr) {
                    valueString += fmt::format("{} = {} | ", bitfield->getVariableName(), bitfield->formatDisplayValue());
                }
            }

            if (valueString.size() >= 3) {
                valueString.resize(valueString.size() - 3);
            }

            if (valueString.size() > 64)
                return Pattern::callUserFormatFunc(this->reference()).value_or(fmt::format("{{ ... }}", valueString));
            else
                return Pattern::callUserFormatFunc(this->reference()).value_or(fmt::format("{{ {} }}", valueString));
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

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) override {
            this->m_fields = entries;
        }

        void setOffset(u64 offset) override {
            for (auto &field : this->m_fields) {
                if (field->getSection() == this->getSection()) {
                    if (field->getSection() != ptrn::Pattern::PatternLocalSectionId)
                        field->setOffset(field->getOffset() - this->getOffset() + offset);
                    else
                        field->setOffset(offset);
                }
            }

            PatternBitfieldMember::setOffset(offset);
        }

        void forEachEntry(u64 start, u64 end, const std::function<void (u64, Pattern *)> &fn) override {
            if (this->isSealed())
                return;

            for (auto i = start; i < end; i++) {
                auto &pattern = this->m_fields[i];
                if (!pattern->isPatternLocal() || pattern->hasAttribute("export"))
                    fn(i, pattern.get());
            }
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedFields.clear();
            for (auto &member : this->m_fields)
                this->m_sortedFields.push_back(member.get());

            std::sort(this->m_sortedFields.begin(), this->m_sortedFields.end(), comparator);
            if (this->isReversed())
                std::reverse(this->m_sortedFields.begin(), this->m_sortedFields.end());

            for (auto &member : this->m_fields)
                member->sort(comparator);
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        void clearFormatCache() override {
            this->forEachEntry(0, this->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                entry->clearFormatCache();
            });

            Pattern::clearFormatCache();
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_fields;
        std::vector<Pattern *> m_sortedFields;

        u8 m_firstBitOffset = 0;
        u64 m_totalBitSize = 0;
        bool m_reversed = false;
    };

}