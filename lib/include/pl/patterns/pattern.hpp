#pragma once

#include <pl/core/errors/error.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/pattern_visitor.hpp>
#include <pl/helpers/types.hpp>
#include <pl/helpers/utils.hpp>

#include <fmt/format.h>

#include <wolv/utils/core.hpp>
#include <wolv/utils/guards.hpp>

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
        [[nodiscard]] virtual std::vector<std::shared_ptr<Pattern>> getEntries() = 0;
        [[nodiscard]] virtual std::shared_ptr<Pattern> getEntry(size_t index) const = 0;
        virtual void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)> &callback) = 0;
        [[nodiscard]] virtual size_t getEntryCount() const = 0;
    };

    enum class Visibility : u8 {
        Visible,
        HighlightHidden,
        Hidden
    };

    class Pattern {
    public:
        constexpr static u64 MainSectionId          = 0x0000'0000'0000'0000;
        constexpr static u64 HeapSectionId          = 0xFFFF'FFFF'FFFF'FFFF;
        constexpr static u64 PatternLocalSectionId  = 0xFFFF'FFFF'FFFF'FFFE;

        Pattern(core::Evaluator *evaluator, u64 offset, size_t size)
            : m_evaluator(evaluator), m_offset(offset), m_size(size) {

            if (evaluator != nullptr) {
                this->m_color       = evaluator->getNextPatternColor();
                this->m_manualColor = false;
                evaluator->patternCreated(this);
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
            this->m_initialized = other.m_initialized;
            this->m_constant = other.m_constant;
            this->m_variableName = other.m_variableName;
            this->m_typeName = other.m_typeName;

            if (other.m_cachedDisplayValue != nullptr)
                this->m_cachedDisplayValue = std::make_unique<std::string>(*other.m_cachedDisplayValue);
            if (other.m_attributes != nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::vector<core::Token::Literal>>>(*other.m_attributes);

            if (this->m_evaluator != nullptr) {
                this->m_evaluator->patternCreated(this);
            }
        }

        virtual ~Pattern() {
            if (this->m_evaluator != nullptr) {
                this->m_evaluator->patternDestroyed(this);
            }
        }

        virtual std::unique_ptr<Pattern> clone() const = 0;

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        [[nodiscard]] virtual u64 getOffsetForSorting() const { return this->getOffset(); }
        [[nodiscard]] u32 getHeapAddress() const { return this->getOffset() >> 32; }
        virtual void setOffset(u64 offset) {
            this->m_offset = offset;
        }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        [[nodiscard]] virtual size_t getSizeForSorting() const { return this->getSize(); }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] std::string getVariableName() const {
            if (this->m_variableName.empty())
                return fmt::format("{} @ 0x{:02X}", this->getTypeName(), this->getOffset());
            else
                return this->m_variableName;
        }
        void setVariableName(const std::string &name) {
            if (!name.empty())
                this->m_variableName = name;
        }

        [[nodiscard]] std::string getComment() const {
            if (const auto &arguments = this->getAttributeArguments("comment"); !arguments.empty())
                return arguments.front().toString(true);
            else
                return "";
        }

        void setComment(const std::string &comment) {
            if (!comment.empty())
                this->addAttribute("comment", { comment });
        }

        [[nodiscard]] virtual std::string getTypeName() const {
            return this->m_typeName;
        }

        void setTypeName(const std::string &name) {
            if (!name.empty())
                this->m_typeName = name;
        }

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
        virtual void setEndian(std::endian endian) {
            if (this->isLocal()) return;

            this->m_endian = endian;
        }
        [[nodiscard]] bool hasOverriddenEndian() const { return this->m_endian.has_value(); }

        [[nodiscard]] std::string getDisplayName() const {
            if (const auto &arguments = this->getAttributeArguments("name"); !arguments.empty())
                return arguments.front().toString(true);
            else
                return this->getVariableName();
        }
        void setDisplayName(const std::string &name) { this->addAttribute("name", { name }); }

        [[nodiscard]] std::string getTransformFunction() const {
            if (const auto &arguments = this->getAttributeArguments("transform"); !arguments.empty())
                return arguments.front().toString(true);
            else
                return "";
        }
        void setTransformFunction(const std::string &functionName) { this->addAttribute("transform", { functionName }); }
        [[nodiscard]] std::string getReadFormatterFunction() const {
            if (const auto &arguments = this->getAttributeArguments("format_read"); !arguments.empty())
                return arguments.front().toString(true);
            else
                return "";
        }
        void setReadFormatterFunction(const std::string &functionName) { this->addAttribute("format_read", { functionName }); }
        [[nodiscard]] std::string getWriteFormatterFunction() const {
            if (const auto &arguments = this->getAttributeArguments("format_write"); !arguments.empty())
                return arguments.front().toString(true);
            else
                return "";
        }
        void setWriteFormatterFunction(const std::string &functionName) { this->addAttribute("format_write", { functionName }); }

        [[nodiscard]] virtual std::string getFormattedName() const = 0;

        [[nodiscard]] std::string getFormattedValue() {
            if (this->m_cachedDisplayValue != nullptr)
                return *this->m_cachedDisplayValue;

            try {
                auto &currOffset = this->m_evaluator->dataOffset();
                auto startOffset = currOffset;
                currOffset = this->getOffset();

                auto savedScope = this->m_evaluator->getScope(0);

                ON_SCOPE_EXIT {
                    this->m_evaluator->getScope(0) = savedScope;
                    currOffset = startOffset;
                };

                return this->formatDisplayValue();
            } catch(std::exception &e) {
                this->m_cachedDisplayValue = std::make_unique<std::string>(e.what());
                return *this->m_cachedDisplayValue;
            }
        }

        [[nodiscard]] virtual std::string toString() const {
            auto result = fmt::format("{} {} @ 0x{:X}", this->getTypeName(), this->getVariableName(), this->getOffset());

            return this->formatDisplayValue(result, this->getValue());
        }

        [[nodiscard]] virtual core::Token::Literal getValue() const {
            auto clone = this->clone();
            return this->transformValue(clone.get());
        }

        [[nodiscard]] virtual std::vector<std::pair<u64, Pattern*>> getChildren() {
            if (this->isPatternLocal())
                return { };
            else
                return { { this->getOffset(), this } };
        }

        void setVisibility(Visibility visibility) {
            switch (visibility) {
                case Visibility::Visible:
                    this->removeAttribute("hidden");
                    this->removeAttribute("highlight_hidden");
                    break;
                case Visibility::Hidden:
                    this->addAttribute("hidden");
                    this->removeAttribute("highlight_hidden");
                    break;
                case Visibility::HighlightHidden:
                    this->removeAttribute("hidden");
                    this->addAttribute("highlight_hidden");
                    break;
            }
        }

        [[nodiscard]] Visibility getVisibility() const {
            if (this->hasAttribute("hidden"))
                return Visibility::Hidden;
            else if (this->hasAttribute("highlight_hidden"))
                return Visibility::HighlightHidden;
            else
                return Visibility::Visible;
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
            if (local) {
                this->setEndian(std::endian::native);
                this->m_section = HeapSectionId;
            } else {
                this->m_section = MainSectionId;
            }
        }

        [[nodiscard]] bool isLocal() const {
            return this->m_section != MainSectionId;
        }

        [[nodiscard]] bool isPatternLocal() const {
            return this->m_section == PatternLocalSectionId;
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
            wolv::util::unused(comparator);
        }

        [[nodiscard]] virtual bool operator!=(const Pattern &other) const final { return !operator==(other); }
        [[nodiscard]] virtual bool operator==(const Pattern &other) const = 0;

        std::vector<u8> getBytes() {
            std::vector<u8> result;
            result.reserve(this->getChildren().size());

            if (!this->getTransformFunction().empty()) {
                auto bytes = std::visit(wolv::util::overloaded {
                        [](u128 value) { return hlp::toMinimalBytes(value); },
                        [](i128 value) { return hlp::toMinimalBytes(value); },
                        [](Pattern *pattern) { return pattern->getBytes(); },
                        [](auto value) { return wolv::util::toContainer<std::vector<u8>>(wolv::util::toBytes(value)); }
                }, this->getValue());
                std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));
            } else if (auto iteratable = dynamic_cast<pl::ptrn::Iteratable*>(this); iteratable != nullptr) {
                iteratable->forEachEntry(0, iteratable->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                    const auto children = entry->getChildren();
                    for (const auto &[offset, child] : children) {
                        auto startOffset = child->getOffset();

                        child->setOffset(offset);
                        ON_SCOPE_EXIT { child->setOffset(startOffset); };

                        auto bytes = child->getBytes();
                        std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));
                    }
                });
            } else {
                result.resize(this->getSize());
                this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            }

            return result;
        }

        virtual std::vector<u8> getBytesOf(const core::Token::Literal &value) const {
            auto bytes = value.toBytes();
            if (this->getEndian() == std::endian::big)
                std::reverse(bytes.begin(), bytes.end());
            bytes.resize(this->getSize());

            return bytes;
        }

        void setValue(const core::Token::Literal &value) {
            std::vector<u8> result;

            const auto &formatterFunctionName = this->getWriteFormatterFunction();
            if (formatterFunctionName.empty()) {
                result = this->getBytesOf(value);
            } else {
                try {
                    const auto function = this->m_evaluator->findFunction(formatterFunctionName);
                    if (function.has_value()) {
                        auto formatterResult = function->func(this->m_evaluator, { value });

                        if (formatterResult.has_value()) {
                            result = this->getBytesOf(*formatterResult);
                        }
                    }
                } catch (core::err::EvaluatorError::Exception &error) {
                    wolv::util::unused(error);
                }
            }

            if (!result.empty()) {
                this->getEvaluator()->writeData(this->getOffset(), result.data(), result.size(), this->getSection());
                this->clearFormatCache();
            }
        }

        void clearFormatCache() {
            if (this->m_cachedDisplayValue == nullptr)
                return;

            this->m_cachedDisplayValue.reset();

            if (auto *iteratable = dynamic_cast<Iteratable*>(this); iteratable != nullptr) {
                for (auto &entry : iteratable->getEntries()) {
                    entry->clearFormatCache();
                }
            }
        }

        virtual void accept(PatternVisitor &v) = 0;

        void addAttribute(const std::string &attribute, const std::vector<core::Token::Literal> &arguments = {}) {
            if (this->m_attributes == nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::vector<core::Token::Literal>>>();

            (*this->m_attributes)[attribute] = arguments;
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

        [[nodiscard]] std::vector<core::Token::Literal> getAttributeArguments(const std::string &name) const {
            if (!this->hasAttribute(name))
                return {};
            else
                return this->m_attributes->at(name);
        }

        void setFormatValue(const std::string &value) {
            this->m_cachedDisplayValue = std::make_unique<std::string>(value);
        }

        [[nodiscard]] core::Evaluator* getEvaluator() const {
            return this->m_evaluator;
        }

        [[nodiscard]] bool isConstant() const {
            return this->m_constant;
        }

        void setConstant(bool constant) {
            this->m_constant = constant;
        }

        [[nodiscard]] bool isInitialized() const {
            return this->m_initialized;
        }

        void setInitialized(bool initialized) {
            this->m_initialized = initialized;
        }


    protected:
        std::optional<std::endian> m_endian;

        [[nodiscard]] core::Token::Literal transformValue(const core::Token::Literal &value) const {
            auto evaluator = this->getEvaluator();
            if (auto transformFunc = evaluator->findFunction(this->getTransformFunction()); transformFunc.has_value())
                if (auto result = transformFunc->func(evaluator, { value }); result.has_value())
                    return *result;

            return value;
        }

        [[nodiscard]] virtual std::string formatDisplayValue() = 0;

        [[nodiscard]] std::string formatDisplayValue(const std::string &value, const core::Token::Literal &literal) const {
            const auto &formatterFunctionName = this->getReadFormatterFunction();
            if (formatterFunctionName.empty())
                return value;
            else {
                try {
                    const auto function = this->m_evaluator->findFunction(formatterFunctionName);
                    if (function.has_value()) {
                        auto result = function->func(this->m_evaluator, { literal });

                        if (result.has_value()) {
                            return result->toString(true);
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

        template<typename T>
        [[nodiscard]] bool compareCommonProperties(const Pattern &other) const {
            return typeid(other) == typeid(std::remove_cvref_t<T>) &&
                   this->m_offset == other.m_offset &&
                   this->m_size == other.m_size &&
                   (this->m_attributes == nullptr || other.m_attributes == nullptr || *this->m_attributes == *other.m_attributes) &&
                   (this->m_endian == other.m_endian || (!this->m_endian.has_value() && other.m_endian == std::endian::native) || (!other.m_endian.has_value() && this->m_endian == std::endian::native)) &&
                   this->m_variableName == other.m_variableName &&
                   this->m_typeName == other.m_typeName &&
                   this->m_section == other.m_section;
        }

    private:
        friend pl::core::Evaluator;

        core::Evaluator *m_evaluator;

        std::unique_ptr<std::map<std::string, std::vector<core::Token::Literal>>> m_attributes;
        mutable std::unique_ptr<std::string> m_cachedDisplayValue;

        std::string m_variableName;
        std::string m_typeName;

        u64 m_offset  = 0x00;
        size_t m_size = 0x00;
        u64 m_section = 0x00;

        u32 m_color = 0x00;

        bool m_reference = false;
        bool m_constant = false;
        bool m_initialized = false;

        bool m_manualColor = false;
    };

}