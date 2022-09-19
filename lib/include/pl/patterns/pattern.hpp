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
        virtual Pattern* getEntry(size_t index) const = 0;
        virtual void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)> &callback) = 0;
        virtual size_t getEntryCount() const = 0;
    };

    class PatternCreationLimiter {
    public:
        explicit PatternCreationLimiter(core::Evaluator *evaluator) : m_evaluator(evaluator) {
            if (getEvaluator() == nullptr) return;

            getEvaluator()->patternCreated();
        }

        PatternCreationLimiter(const PatternCreationLimiter &other) : PatternCreationLimiter(other.m_evaluator) { }

        virtual ~PatternCreationLimiter() {
            if (getEvaluator() == nullptr) return;

            getEvaluator()->patternDestroyed();
        }

        [[nodiscard]] core::Evaluator *getEvaluator() const {
            return this->m_evaluator;
        }

    private:
        core::Evaluator *m_evaluator = nullptr;
    };

    class Pattern : public PatternCreationLimiter,
                    public Cloneable<Pattern> {
    public:
        Pattern(core::Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : PatternCreationLimiter(evaluator), m_offset(offset), m_size(size), m_color(color) {

            if (color != 0)
                return;

            if (evaluator != nullptr) {
                this->m_color       = evaluator->getNextPatternColor();
                this->m_manualColor = false;
            }
        }

        Pattern(const Pattern &other) : PatternCreationLimiter(other) {
            this->m_offset = other.m_offset;
            this->m_endian = other.m_endian;
            this->m_local = other.m_local;
            this->m_variableName = other.m_variableName;
            this->m_size = other.m_size;
            this->m_color = other.m_color;
            this->m_cachedDisplayValue = other.m_cachedDisplayValue;
            this->m_hidden = other.m_hidden;
            this->m_typeName = other.m_typeName;
            this->m_manualColor = other.m_manualColor;
            this->m_sealed = other.m_sealed;

            if (other.m_comment != nullptr)
                this->m_comment = std::make_unique<std::string>(*other.m_comment);
            if (other.m_displayName != nullptr)
                this->m_displayName = std::make_unique<std::string>(*other.m_displayName);
            if (other.m_attributes != nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::string>>(*other.m_attributes);

            if (other.m_formatterFunction != nullptr)
                this->m_formatterFunction = std::make_unique<api::Function>(*other.m_formatterFunction);
            if (other.m_transformFunction != nullptr)
                this->m_transformFunction = std::make_unique<api::Function>(*other.m_transformFunction);
        }

        ~Pattern() override = default;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        [[nodiscard]] u32 getHeapAddress() const { return this->getOffset() >> 32; }
        virtual void setOffset(u64 offset) {
            this->m_offset = offset;
        }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] const std::string &getVariableName() const { return this->m_variableName; }
        void setVariableName(std::string name) { this->m_variableName = std::move(name); }

        [[nodiscard]] const auto& getComment() const { return this->m_comment; }
        void setComment(std::string comment) { this->m_comment = std::make_unique<std::string>(std::move(comment)); }

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

        [[nodiscard]] std::string getDisplayName() const { return this->m_displayName == nullptr ? this->m_variableName : *this->m_displayName; }
        void setDisplayName(const std::string &name) { this->m_displayName = std::make_unique<std::string>(name); }

        [[nodiscard]] const auto &getTransformFunction() const { return this->m_transformFunction; }
        void setTransformFunction(const api::Function &function) { this->m_transformFunction = std::make_unique<api::Function>(function); }
        [[nodiscard]] const auto &getFormatterFunction() const { return this->m_formatterFunction; }
        void setFormatterFunction(const api::Function &function) { this->m_formatterFunction = std::make_unique<api::Function>(function); }

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

        virtual void setLocal(bool local) {
            this->m_local = local;
        }

        [[nodiscard]] bool isLocal() const {
            return this->m_local;
        }

        virtual void setReference(bool reference) {
            this->m_reference = reference;
        }

        [[nodiscard]] bool isReference() const {
            return this->m_reference;
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
                   this->m_hidden == other.m_hidden &&
                   (this->m_endian == other.m_endian || (!this->m_endian.has_value() && other.m_endian == std::endian::native) || (!other.m_endian.has_value() && this->m_endian == std::endian::native)) &&
                   this->m_variableName == other.m_variableName &&
                   this->m_typeName == other.m_typeName &&
                   this->m_comment == other.m_comment &&
                   this->m_local == other.m_local;
        }

        [[nodiscard]] std::string calcDisplayValue(const std::string &value, const core::Token::Literal &literal) const {
            const auto &formatterFunction = this->getFormatterFunction();
            if (formatterFunction == nullptr)
                return value;
            else {
                try {
                    auto result = formatterFunction->func(this->getEvaluator(), { literal });

                    if (result.has_value()) {
                        return core::Token::literalToString(*result, true);
                    } else {
                        return "";
                    }
                } catch (core::err::EvaluatorError::Exception &error) {
                    return error.what();
                }
            }
        }

        [[nodiscard]] std::string formatDisplayValue(const std::string &value, const core::Token::Literal &literal) const {
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

        void addAttribute(const std::string &attribute, const std::string &value) {
            if (this->m_attributes == nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::string>>();

            this->m_attributes->emplace(attribute, value);
        }

        const auto &getAttributes() const {
            return this->m_attributes;
        }

        void setFormatValue(const std::string &value) {
            this->m_cachedDisplayValue = value;
        }

    protected:
        std::optional<std::endian> m_endian;
        bool m_hidden = false;
        bool m_sealed = false;

    private:
        friend pl::core::Evaluator;

        u64 m_offset  = 0x00;
        size_t m_size = 0x00;

        u32 m_color = 0x00;
        std::unique_ptr<std::string> m_displayName;
        mutable std::optional<std::string> m_cachedDisplayValue;
        std::string m_variableName;
        std::unique_ptr<std::string> m_comment;
        std::string m_typeName;
        std::unique_ptr<std::map<std::string, std::string>> m_attributes;

        std::unique_ptr<api::Function> m_formatterFunction;
        std::unique_ptr<api::Function> m_transformFunction;

        bool m_local = false;
        bool m_reference = false;

        bool m_manualColor = false;
    };

}