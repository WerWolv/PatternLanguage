#pragma once

#include <pl/core/errors/error.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/pattern_visitor.hpp>
#include <pl/helpers/guards.hpp>
#include <pl/helpers/types.hpp>
#include <pl/helpers/utils.hpp>
#include <fmt/format.h>

#include <string>

namespace pl::ptrn {

    using namespace ::std::literals::string_literals;

    class Inlinable {
    public:
        [[nodiscard]] bool isInlined() const { return this->m_inlined; }
        void setInlined(bool inlined) { this->m_inlined = inlined; }

    private:
        bool m_inlined = false;
    };

    class Iteratable {
    public:
        [[nodiscard]] virtual std::shared_ptr<Pattern> getEntry(size_t index) const = 0;
        virtual void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)> &callback) = 0;
        [[nodiscard]] virtual size_t getEntryCount() const = 0;
    };

    class Pattern {
    public:
        constexpr static u64 MainSectionId = 0x0000'0000'0000'0000;
        constexpr static u64 HeapSectionId = 0xFFFF'FFFF'FFFF'FFFF;

        Pattern(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : m_evaluator(evaluator), m_offset(offset), m_size(size), m_color(color) {

            if (color != 0)
                return;

            if (evaluator != nullptr) {
                this->m_color       = evaluator->getNextPatternColor();
                this->m_manualColor = false;
                evaluator->patternCreated();
            }
        }

        Pattern(const Pattern &other) {
            this->m_evaluator = other.m_evaluator;
            this->m_offset = other.m_offset;
            this->m_endian = other.m_endian;
            this->m_size = other.m_size;
            this->m_color = other.m_color;
            this->m_manualColor = other.m_manualColor;
            this->m_section = other.m_section;

            if (other.m_variableName != nullptr)
                this->m_variableName = std::make_unique<std::string>(*other.m_variableName);
            if (other.m_typeName != nullptr)
                this->m_typeName = std::make_unique<std::string>(*other.m_typeName);
            if (other.m_cachedDisplayValue != nullptr)
                this->m_cachedDisplayValue = std::make_unique<std::string>(*other.m_cachedDisplayValue);
            if (other.m_attributes != nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::string>>(*other.m_attributes);

            if (this->m_evaluator != nullptr) {
                this->m_evaluator->patternCreated();
            }
        }

        virtual ~Pattern() {
            if (this->m_evaluator != nullptr)
                this->m_evaluator->patternDestroyed();
        }

        virtual std::unique_ptr<Pattern> clone() const = 0;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        [[nodiscard]] u32 getHeapAddress() const { return this->getOffset() >> 32; }
        virtual void setOffset(u64 offset) {
            this->m_offset = offset;
        }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] std::string getVariableName() const {
            if (this->m_variableName == nullptr)
                return fmt::format("{} @ 0x{:02X}", this->getTypeName(), this->getOffset());
            else
                return *this->m_variableName;
        }
        void setVariableName(std::string name) {
            this->m_variableName = std::make_unique<std::string>(std::move(name));
        }

        [[nodiscard]] auto getComment() const { return this->getAttributeValue("comment").value_or(""); }
        void setComment(const std::string &comment) { this->addAttribute("comment", comment); }

        [[nodiscard]] virtual std::string getTypeName() const { return this->m_typeName == nullptr ? "" : *this->m_typeName; }
        void setTypeName(std::string name) { this->m_typeName = std::make_unique<std::string>(std::move(name)); }

        [[nodiscard]] u32 getColor() const { return this->m_color; }
        virtual void setColor(u32 color) {
            this->m_color       = color;
            this->m_manualColor = true;
        }
        void setBaseColor(u32 color) {
            if (this->hasOverriddenColor())
                this->setColor(color);
            else
                this->m_color = color;
        }
        [[nodiscard]] bool hasOverriddenColor() const { return this->m_manualColor; }

        [[nodiscard]] std::endian getEndian() const {
            if (this->m_evaluator == nullptr) return std::endian::native;
            else return this->m_endian.value_or(this->m_evaluator->getDefaultEndian());
        }
        virtual void setEndian(std::endian endian) { this->m_endian = endian; }
        [[nodiscard]] bool hasOverriddenEndian() const { return this->m_endian.has_value(); }

        [[nodiscard]] std::string getDisplayName() const { return this->getAttributeValue("name").value_or(this->getVariableName()); }
        void setDisplayName(const std::string &name) { this->addAttribute("name", name); }

        [[nodiscard]] auto getTransformFunction() const { return this->getAttributeValue("transform").value_or(""); }
        void setTransformFunction(const std::string &functionName) { this->addAttribute("transform", functionName); }
        [[nodiscard]] auto getFormatterFunction() const { return this->getAttributeValue("format").value_or(""); }
        void setFormatterFunction(const std::string &functionName) { this->addAttribute("format", functionName); }

        [[nodiscard]] virtual std::string getFormattedName() const = 0;
        [[nodiscard]] virtual std::string getFormattedValue() = 0;

        [[nodiscard]] virtual std::string toString() const {
            auto result = fmt::format("{} {} @ 0x{:X}", this->getTypeName(), this->getVariableName(), this->getOffset());

            return this->formatDisplayValue(result, this->getValue());
        }

        [[nodiscard]] virtual core::Token::Literal getValue() const {
            return u128();
        }

        [[nodiscard]] virtual std::vector<std::pair<u64, Pattern*>> getChildren() {
            return { { this->getOffset(), this } };
        }

        void setHidden(bool hidden) {
            if (hidden)
                this->addAttribute("hidden");
            else
                this->removeAttribute("hidden");
        }

        [[nodiscard]] bool isHidden() const {
            return this->hasAttribute("hidden");
        }

        void setSealed(bool sealed) {
            if (sealed)
                this->addAttribute("sealed");
            else
                this->removeAttribute("sealed");
        }

        [[nodiscard]] bool isSealed() const {
            return this->hasAttribute("sealed");
        }

        virtual void setLocal(bool local) {
            this->m_section = local ? HeapSectionId : MainSectionId;
        }

        [[nodiscard]] bool isLocal() const {
            return this->m_section != MainSectionId;
        }

        virtual void setReference(bool reference) {
            this->m_reference = reference;
        }

        [[nodiscard]] bool isReference() const {
            return this->m_reference;
        }

        virtual void setSection(u64 id) {
            this->m_section = id;
        }

        [[nodiscard]] u64 getSection() const {
            return this->m_section;
        }

        virtual void sort(const std::function<bool(const Pattern *left, const Pattern *right)> &comparator) {
            hlp::unused(comparator);
        }

        [[nodiscard]] virtual bool operator!=(const Pattern &other) const final { return !operator==(other); }
        [[nodiscard]] virtual bool operator==(const Pattern &other) const = 0;

        template<typename T>
        [[nodiscard]] bool areCommonPropertiesEqual(const Pattern &other) const {
            return typeid(other) == typeid(std::remove_cvref_t<T>) &&
                   this->m_offset == other.m_offset &&
                   this->m_size == other.m_size &&
                   (this->m_attributes == nullptr || other.m_attributes == nullptr || *this->m_attributes == *other.m_attributes) &&
                   (this->m_endian == other.m_endian || (!this->m_endian.has_value() && other.m_endian == std::endian::native) || (!other.m_endian.has_value() && this->m_endian == std::endian::native)) &&
                   (this->m_variableName == nullptr || other.m_variableName == nullptr || *this->m_variableName == *other.m_variableName) &&
                   (this->m_typeName == nullptr || other.m_typeName == nullptr || *this->m_typeName == *other.m_typeName) &&
                   this->m_section == other.m_section;
        }

        [[nodiscard]] std::string calcDisplayValue(const std::string &value, const core::Token::Literal &literal) const {
            const auto &formatterFunctionName = this->getFormatterFunction();
            if (formatterFunctionName.empty())
                return value;
            else {
                try {
                    const auto function = this->m_evaluator->findFunction(formatterFunctionName);
                    if (function.has_value()) {
                        auto result = function->func(this->m_evaluator, { literal });

                        if (result.has_value()) {
                            return core::Token::literalToString(*result, true);
                        } else {
                            return "";
                        }
                    } else {
                        return "";
                    }

                } catch (core::err::EvaluatorError::Exception &error) {
                    return error.what();
                }
            }
        }

        [[nodiscard]] std::string formatDisplayValue(const std::string &value, const core::Token::Literal &literal) const {
            if (this->m_cachedDisplayValue == nullptr) {
                auto &currOffset = this->m_evaluator->dataOffset();
                auto startOffset = currOffset;
                currOffset = this->getOffset();

                auto savedScope = this->m_evaluator->getScope(0);
                this->m_cachedDisplayValue = std::make_unique<std::string>(calcDisplayValue(value, literal));
                this->m_evaluator->getScope(0) = savedScope;

                currOffset = startOffset;
            }

            return *this->m_cachedDisplayValue;
        }

        void clearFormatCache() {
            this->m_cachedDisplayValue.reset();
        }

        virtual void accept(PatternVisitor &v) = 0;

        void addAttribute(const std::string &attribute, const std::string &value = "") {
            if (this->m_attributes == nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::string>>();

            (*this->m_attributes)[attribute] = value;
        }

        void removeAttribute(const std::string &attribute) {
            if (this->m_attributes != nullptr)
                this->m_attributes->erase(attribute);
        }

        [[nodiscard]] bool hasAttribute(const std::string &attribute) const {
            if (this->m_attributes == nullptr)
                return false;

            return this->m_attributes->contains(attribute);
        }

        [[nodiscard]] const auto &getAttributes() const {
            return this->m_attributes;
        }

        [[nodiscard]] std::optional<std::string> getAttributeValue(const std::string &name) const {
            if (!this->hasAttribute(name))
                return std::nullopt;
            else
                return this->m_attributes->at(name);
        }

        void setFormatValue(const std::string &value) {
            this->m_cachedDisplayValue = std::make_unique<std::string>(value);
        }

        [[nodiscard]] auto getEvaluator() const {
            return this->m_evaluator;
        }

    protected:
        std::optional<std::endian> m_endian;

    private:
        friend pl::core::Evaluator;

        core::Evaluator *m_evaluator;

        std::unique_ptr<std::map<std::string, std::string>> m_attributes;
        mutable std::unique_ptr<std::string> m_cachedDisplayValue;

        std::unique_ptr<std::string> m_variableName;
        std::unique_ptr<std::string> m_typeName;

        u64 m_offset  = 0x00;
        size_t m_size = 0x00;
        u64 m_section = 0x00;

        u32 m_color = 0x00;

        bool m_reference = false;

        bool m_manualColor = false;
    };

}