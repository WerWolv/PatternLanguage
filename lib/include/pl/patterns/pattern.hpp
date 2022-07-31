#pragma once

#include <helpers/types.hpp>

#include <pl/evaluator.hpp>
#include <pl/pattern_visitor.hpp>
#include <helpers/utils.hpp>
#include <fmt/format.h>

#include <string>

namespace pl {

    using namespace ::std::literals::string_literals;

    class Inlinable {
    public:
        [[nodiscard]] bool isInlined() const { return this->m_inlined; }
        void setInlined(bool inlined) { this->m_inlined = inlined; }

    private:
        bool m_inlined = false;
    };

    enum class PatternMemoryType : u8 {
        Provider,
        Stack,
        Heap
    };

    class PatternCreationLimiter {
    public:
        explicit PatternCreationLimiter(Evaluator *evaluator) : m_evaluator(evaluator) {
            if (getEvaluator() == nullptr) return;

            getEvaluator()->patternCreated();
        }

        PatternCreationLimiter(const PatternCreationLimiter &other) : PatternCreationLimiter(other.m_evaluator) { }

        virtual ~PatternCreationLimiter() {
            if (getEvaluator() == nullptr) return;

            getEvaluator()->patternDestroyed();
        }

        [[nodiscard]] Evaluator *getEvaluator() const {
            return this->m_evaluator;
        }

    private:
        Evaluator *m_evaluator = nullptr;
    };

    class Pattern : public PatternCreationLimiter,
                    public Cloneable<Pattern> {
    public:
        Pattern(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternCreationLimiter(evaluator), m_offset(offset), m_size(size), m_color(color) {

            if (color != 0)
                return;

            if (evaluator != nullptr) {
                this->m_color       = evaluator->getNextPatternColor();
                this->m_manualColor = false;
            }
        }

        Pattern(const Pattern &other) = default;

        ~Pattern() override = default;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        virtual void setOffset(u64 offset) {
            this->m_offset = offset;

            if (this->m_memoryType == PatternMemoryType::Heap)
                this->m_heapAddressValid = true;
        }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] const std::string &getVariableName() const { return this->m_variableName; }
        void setVariableName(std::string name) { this->m_variableName = std::move(name); }

        [[nodiscard]] const std::optional<std::string> &getComment() const { return this->m_comment; }
        void setComment(std::string comment) { this->m_comment = std::move(comment); }

        [[nodiscard]] virtual std::string getTypeName() const { return this->m_typeName; }
        void setTypeName(std::string name) { this->m_typeName = std::move(name); }

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
            if (this->getEvaluator() == nullptr) return std::endian::native;
            else return this->m_endian.value_or(this->getEvaluator()->getDefaultEndian());
        }
        virtual void setEndian(std::endian endian) { this->m_endian = endian; }
        [[nodiscard]] bool hasOverriddenEndian() const { return this->m_endian.has_value(); }

        [[nodiscard]] std::string getDisplayName() const { return this->m_displayName.value_or(this->m_variableName); }
        void setDisplayName(const std::string &name) { this->m_displayName = name; }

        [[nodiscard]] const auto &getTransformFunction() const { return this->m_transformFunction; }
        void setTransformFunction(const api::Function &function) { this->m_transformFunction = function; }
        [[nodiscard]] const auto &getFormatterFunction() const { return this->m_formatterFunction; }
        void setFormatterFunction(const api::Function &function) { this->m_formatterFunction = function; }

        [[nodiscard]] virtual std::string getFormattedName() const = 0;
        [[nodiscard]] virtual std::string getFormattedValue() = 0;

        [[nodiscard]] virtual std::string toString() const {
            return fmt::format("{} {} @ 0x{:X}", this->getTypeName(), this->getVariableName(), this->getOffset());
        }

        [[nodiscard]] virtual std::vector<std::pair<u64, Pattern*>> getChildren() {
            return { { this->getOffset(), this } };
        }

        void setHidden(bool hidden) {
            this->m_hidden = hidden;
        }

        [[nodiscard]] bool isHidden() const {
            return this->m_hidden;
        }

        void setSealed(bool sealed) {
            this->m_sealed = sealed;
        }

        [[nodiscard]] bool isSealed() const {
            return this->m_sealed;
        }

        virtual void setMemoryLocationType(PatternMemoryType type) {
            this->m_memoryType = type;
            this->m_heapAddressValid = false;
        }

        [[nodiscard]] bool isHeapAddressValid() const {
            return this->m_heapAddressValid;
        }

        [[nodiscard]] bool isLocal() const {
            return this->m_memoryType != PatternMemoryType::Provider;
        }

        [[nodiscard]] PatternMemoryType getMemoryLocationType() const {
            return this->m_memoryType;
        }

        virtual void sort(const std::function<bool(const Pattern *left, const Pattern *right)> &comparator) {
            pl::unused(comparator);
        }

        [[nodiscard]] virtual bool operator!=(const Pattern &other) const final { return !operator==(other); }
        [[nodiscard]] virtual bool operator==(const Pattern &other) const = 0;

        template<typename T>
        [[nodiscard]] bool areCommonPropertiesEqual(const Pattern &other) const {
            return typeid(other) == typeid(std::remove_cvref_t<T>) &&
                   this->m_offset == other.m_offset &&
                   this->m_size == other.m_size &&
                   this->m_hidden == other.m_hidden &&
                   (this->m_endian == other.m_endian || (!this->m_endian.has_value() && other.m_endian == std::endian::native) || (!other.m_endian.has_value() && this->m_endian == std::endian::native)) &&
                   this->m_variableName == other.m_variableName &&
                   this->m_typeName == other.m_typeName &&
                   this->m_comment == other.m_comment &&
                   this->m_memoryType == other.m_memoryType;
        }

        [[nodiscard]] std::string calcDisplayValue(const std::string &value, const Token::Literal &literal) const {
            if (!this->m_formatterFunction.has_value())
                return value;
            else {
                try {
                    auto result = this->m_formatterFunction->func(this->getEvaluator(), { literal });

                    if (result.has_value()) {
                        if (auto displayValue = std::get_if<std::string>(&result.value()); displayValue != nullptr)
                            return *displayValue;
                        else
                            return "???";
                    } else {
                        return "???";
                    }
                } catch (PatternLanguageError &error) {
                    return "Error: "s + error.what();
                }
            }
        }

        [[nodiscard]] std::string formatDisplayValue(const std::string &value, const Token::Literal &literal) const {
            if (!this->m_cachedDisplayValue.has_value()) {
                auto &currOffset = this->getEvaluator()->dataOffset();
                auto startOffset = currOffset;
                currOffset = this->getOffset();

                this->m_cachedDisplayValue = calcDisplayValue(value, literal);

                currOffset = startOffset;
            }

            return this->m_cachedDisplayValue.value();
        }

        void clearFormatCache() {
            this->m_cachedDisplayValue.reset();
        }

        virtual void accept(PatternVisitor &v) = 0;

    protected:
        std::optional<std::endian> m_endian;
        bool m_hidden = false;
        bool m_sealed = false;

    private:
        friend Evaluator;

        u64 m_offset  = 0x00;
        size_t m_size = 0x00;

        u32 m_color = 0x00;
        std::optional<std::string> m_displayName;
        mutable std::optional<std::string> m_cachedDisplayValue;
        std::string m_variableName;
        std::optional<std::string> m_comment;
        std::string m_typeName;

        std::optional<api::Function> m_formatterFunction;
        std::optional<api::Function> m_transformFunction;

        PatternMemoryType m_memoryType = PatternMemoryType::Provider;
        bool m_heapAddressValid = false;

        bool m_manualColor = false;
    };

}