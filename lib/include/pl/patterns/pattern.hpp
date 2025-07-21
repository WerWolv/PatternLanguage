#pragma once

#include <pl/core/errors/error.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/core/location.hpp>
#include <pl/pattern_visitor.hpp>
#include <pl/helpers/types.hpp>
#include <pl/helpers/utils.hpp>

#include <fmt/core.h>

#include <wolv/utils/core.hpp>
#include <wolv/utils/guards.hpp>

#include <concepts>
#include <string>

namespace pl::ptrn {
    using namespace ::std::literals::string_literals;

    class IInlinable {
    public:
        [[nodiscard]] bool isInlined() const { return this->m_inlined; }
        void setInlined(bool inlined) { this->m_inlined = inlined; }

    private:
        bool m_inlined = false;
    };

    class IIterable {
    public:
        virtual ~IIterable() = default;
        [[nodiscard]] virtual std::vector<std::shared_ptr<Pattern>> getEntries() = 0;
        virtual void setEntries(const std::vector<std::shared_ptr<Pattern>> &entries) = 0;

        [[nodiscard]] virtual std::shared_ptr<Pattern> getEntry(size_t index) const = 0;
        virtual void forEachEntry(u64 start, u64 end, const std::function<void(u64, Pattern*)> &callback) = 0;

        [[nodiscard]] virtual size_t getEntryCount() const = 0;

        virtual void addEntry(const std::shared_ptr<Pattern> &) {
            core::err::E0012.throwError("Cannot add entry to this pattern");
        }
    };

    enum class Visibility : u8 {
        Visible,
        HighlightHidden,
        TreeHidden,
        Hidden
    };

    class IIndexable : public IIterable {
    public:
        ~IIndexable() override = default;

        using IIterable::getEntries;
        using IIterable::getEntry;
        using IIterable::forEachEntry;
        using IIterable::getEntryCount;

        friend class core::Evaluator;
    };

    class Pattern : public std::enable_shared_from_this<Pattern> {
    public:
        constexpr static u64 MainSectionId          = 0x0000'0000'0000'0000;
        constexpr static u64 HeapSectionId          = 0xFFFF'FFFF'FFFF'FFFF;
        constexpr static u64 PatternLocalSectionId  = 0xFFFF'FFFF'FFFF'FFFE;
        constexpr static u64 InstantiationSectionId = 0xFFFF'FFFF'FFFF'FFFD;

        Pattern(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : m_evaluator(evaluator), m_line(line), m_variableLocation(pl::core::Location::Empty()),
              m_offset(offset), m_size(size) {
            if (evaluator != nullptr) {
                this->m_color       = evaluator->getNextPatternColor();
                this->m_manualColor = false;
                this->m_variableName = m_evaluator->getStringPool().end();
                this->m_typeName = m_evaluator->getStringPool().end();

                evaluator->patternCreated(this);
            }

        }

        Pattern(const Pattern &other) : std::enable_shared_from_this<Pattern>(other) {
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
            this->m_variableLocation = other.m_variableLocation;
            this->m_typeName = other.m_typeName;
            this->m_reference = other.m_reference;
            this->m_parent = other.m_parent;
            this->m_arrayIndex = other.m_arrayIndex;
            this->m_line = other.m_line;

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

        virtual std::shared_ptr<Pattern> clone() const = 0;
        std::shared_ptr<Pattern> reference() {
            auto weakPtr = weak_from_this();
            if (weakPtr.expired()) {
                core::err::E0001.throwError("Cannot call shared_from_this if this is not shared.");
            }
            return shared_from_this();
        }

        [[nodiscard]] u64 getOffset() const { return this->m_offset; }
        [[nodiscard]] virtual u128 getOffsetForSorting() const { return this->getOffset() << 3; }
        [[nodiscard]] u32 getHeapAddress() const { return this->getOffset() >> 32; }
        void setAbsoluteOffset(u64 offset) {
            if (this->m_offset != offset) {
                if (this->m_evaluator != nullptr)
                    this->m_evaluator->patternDestroyed(this);
                this->m_offset = offset;
                if (this->m_evaluator != nullptr)
                    this->m_evaluator->patternCreated(this);
            }
        }
        virtual void setOffset(u64 offset) {
            setAbsoluteOffset(offset);
        }

        [[nodiscard]] size_t getSize() const { return this->m_size; }
        [[nodiscard]] virtual u128 getSizeForSorting() const { return this->getSize() << 3; }
        void setSize(size_t size) { this->m_size = size; }

        [[nodiscard]] std::string getVariableName() const {
            if (!hasVariableName()) {
                if (this->m_arrayIndex.has_value())
                    return fmt::format("[{}]", m_arrayIndex.value());
                else
                    return fmt::format("{} @ 0x{:02X}", this->getTypeName(), this->getOffset());
            } else
                return *this->m_variableName;
        }

        pl::core::Location getVariableLocation() const {
            return m_variableLocation;
        }

        [[nodiscard]] bool hasVariableName() const {
            return getEvaluator()->isStringPoolEntryValid(this->m_variableName);
        }

        void setVariableName(const std::string &name, pl::core::Location loc = pl::core::Location()) {
            if (!name.empty()) {
                auto [it, inserted] = m_evaluator->getStringPool().emplace(name);
                this->m_variableName = it;
                this->m_variableLocation = loc;
            }
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
            if (!getEvaluator()->isStringPoolEntryValid(this->m_typeName))
                return "";
            else
                return *this->m_typeName;
        }

        void setTypeName(const std::string &name) {
            if (!name.empty()) {
                auto [it, inserted] = m_evaluator->getStringPool().emplace(name);
                this->m_typeName = it;
            }
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
            if (m_section == HeapSectionId || m_section == PatternLocalSectionId || m_section == InstantiationSectionId)
                return;

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
                auto startOffset = this->m_evaluator->getReadOffset();
                this->m_evaluator->setReadOffset(this->getOffset());

                ON_SCOPE_EXIT {
                    this->m_evaluator->setReadOffset(startOffset);
                };

                auto result = this->formatDisplayValue();
                this->m_cachedDisplayValue = std::make_unique<std::string>(result);
                this->m_validDisplayValue = true;

                return result;
            } catch(std::exception &e) {
                this->m_cachedDisplayValue = std::make_unique<std::string>(e.what());
                this->m_validDisplayValue = false;

                return *this->m_cachedDisplayValue;
            }
        }

        [[nodiscard]] bool hasValidFormattedValue() const {
            return this->m_validDisplayValue;
        }

        [[nodiscard]] virtual std::string toString() {
            auto result = fmt::format("{} {} @ 0x{:X}", this->getTypeName(), this->getVariableName(), this->getOffset());

            try {
                return this->callUserFormatFunc(this->getValue(), true).value_or(result);
            } catch (std::exception &e) {
                return e.what();
            }
        }

        [[nodiscard]] virtual core::Token::Literal getValue() const {
            auto pattern = this->clone();

            return this->transformValue(std::move(pattern));
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
                    this->removeAttribute("tree_hidden");
                    break;
                case Visibility::Hidden:
                    this->addAttribute("hidden");
                    this->removeAttribute("highlight_hidden");
                    this->removeAttribute("tree_hidden");
                    break;
                case Visibility::HighlightHidden:
                    this->removeAttribute("hidden");
                    this->addAttribute("highlight_hidden");
                    this->removeAttribute("tree_hidden");
                    break;
                case Visibility::TreeHidden:
                    this->removeAttribute("hidden");
                    this->removeAttribute("highlight_hidden");
                    this->addAttribute("tree_hidden");
                    break;
            }
        }

        [[nodiscard]] Visibility getVisibility() const {
            if (this->hasAttribute("hidden"))
                return Visibility::Hidden;
            else if (this->hasAttribute("highlight_hidden"))
                return Visibility::HighlightHidden;
            else if (this->hasAttribute("tree_hidden"))
                return Visibility::TreeHidden;
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
            return this->hasAttribute("sealed") || this->getVisibility() == Visibility::Hidden;
        }

        virtual void setLocal(bool local) {
            if (local) {
                this->setEndian(std::endian::native);
                this->setSection(HeapSectionId);
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
            if (this->m_section == id)
                return;

            if (this->m_section != PatternLocalSectionId && this->m_section != HeapSectionId) {
                if (this->m_evaluator != nullptr)
                    this->m_evaluator->patternDestroyed(this);
                this->m_section = id;
                if (this->m_evaluator != nullptr)
                    this->m_evaluator->patternCreated(this);
            }
        }

        [[nodiscard]] u64 getSection() const {
            return this->m_section;
        }

        virtual void sort(const std::function<bool(const Pattern *left, const Pattern *right)> &comparator) {
            wolv::util::unused(comparator);
        }

        [[nodiscard]] virtual bool operator!=(const Pattern &other) const final { return !operator==(other); }
        [[nodiscard]] virtual bool operator==(const Pattern &other) const = 0;

        virtual std::vector<u8> getRawBytes() = 0;
        const std::vector<u8>& getBytes() {
            if (this->m_cachedBytes != nullptr)
                return *this->m_cachedBytes;

            std::vector<u8> result;
            if (!this->getTransformFunction().empty()) {
                result = std::visit(wolv::util::overloaded {
                    [this](std::integral auto value) {
                        auto bytes = hlp::toMinimalBytes(value);
                        if (this->getEndian() != std::endian::native)
                            std::reverse(bytes.begin(), bytes.end());
                        return bytes;
                    },
                    [](i128 value) {
                        return hlp::toMinimalBytes(value);
                    },
                    [](Pattern *pattern) { return pattern->getRawBytes(); },
                    [this](auto value) {
                        auto bytes = wolv::util::toContainer<std::vector<u8>>(wolv::util::toBytes(value));
                        if (this->getEndian() != std::endian::native)
                            std::reverse(bytes.begin(), bytes.end());
                        return bytes;
                    }
                }, this->getValue());
            } else {
                result = this->getRawBytes();
            }

            this->m_cachedBytes = std::make_unique<std::vector<u8>>(std::move(result));

            return *this->m_cachedBytes;
        }

        virtual std::vector<u8> getBytesOf(const core::Token::Literal &value) const {
            auto bytes = value.toBytes();
            bytes.resize(this->getSize());
            if (this->getEndian() == std::endian::big)
                std::reverse(bytes.begin(), bytes.end());

            return bytes;
        }

        virtual void setValue(const core::Token::Literal &value) {
            std::vector<u8> result;

            const auto &formatterFunctionName = this->getWriteFormatterFunction();
            if (formatterFunctionName.empty()) {
                result = this->getBytesOf(value);
            } else {
                try {
                    const auto function = this->m_evaluator->findFunction(formatterFunctionName);
                    if (function.has_value()) {
                        auto startHeap = this->m_evaluator->getHeap();
                        ON_SCOPE_EXIT { this->m_evaluator->getHeap() = startHeap; };

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

        virtual void clearFormatCache() {
            this->m_cachedDisplayValue.reset();
        }

        void clearByteCache() {
            if (this->m_cachedBytes == nullptr)
                return;

            this->m_cachedBytes.reset();

            if (auto *iterable = dynamic_cast<IIterable*>(this); iterable != nullptr) [[unlikely]] {
                iterable->forEachEntry(0, iterable->getEntryCount(), [](u64, Pattern *pattern) {
                    pattern->clearByteCache();
                });
            }
        }

        virtual void accept(PatternVisitor &v) = 0;

        void addAttribute(const std::string &attribute, const std::vector<core::Token::Literal> &arguments = {}) {
            if (this->m_attributes == nullptr)
                this->m_attributes = std::make_unique<std::map<std::string, std::vector<core::Token::Literal>>>();

            (*this->m_attributes)[attribute] = arguments;
            getEvaluator()->addAttributedPattern(attribute, this);
        }

        void removeAttribute(const std::string &attribute) {
            if (this->m_attributes != nullptr)
                this->m_attributes->erase(attribute);
            getEvaluator()->removeAttributedPattern(attribute, this);
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

        [[nodiscard]] const Pattern* getParent() const {
            return m_parent.lock().get();
        }

        [[nodiscard]] Pattern* getParent() {
            return m_parent.lock().get();
        }

        void setParent(std::shared_ptr<Pattern> parent) {
            m_parent = parent;
        }

        [[nodiscard]] u32 getLine() const { return m_line; }

        void setArrayIndex(u64 index) {
            m_arrayIndex = index;
        }

        [[nodiscard]] virtual std::string formatDisplayValue() = 0;

    protected:
        std::optional<std::endian> m_endian;

        [[nodiscard]] core::Token::Literal transformValue(const core::Token::Literal &value) const {
            auto evaluator = this->getEvaluator();

            if (auto transformFunc = evaluator->findFunction(this->getTransformFunction()); transformFunc.has_value()) {
                auto startHeap = this->m_evaluator->getHeap();
                ON_SCOPE_EXIT { this->m_evaluator->getHeap() = startHeap; };

                if (auto result = transformFunc->func(evaluator, { value }); result.has_value())
                    return *result;
            }

            return value;
        }

        /**
         * @brief Calls an user-defined PL function to format the given literal (pattern), if existing. Else, returns defaultValue
        */
        [[nodiscard]] std::optional<std::string> callUserFormatFunc(const core::Token::Literal &literal, bool fromCast = false) const {
            const auto &formatterFunctionName = this->getReadFormatterFunction();
            if (formatterFunctionName.empty())
                return {};
            else {
                const auto function = this->m_evaluator->findFunction(formatterFunctionName);
                if (function.has_value()) {
                    auto startHeap = this->m_evaluator->getHeap();
                    ON_SCOPE_EXIT { this->m_evaluator->getHeap() = startHeap; };

                    auto result = function->func(this->m_evaluator, { literal });
                    if (result.has_value()) {
                        if (fromCast && result->isPattern() && result->toPattern()->getTypeName() == this->getTypeName()) {
                            return {};
                        } else {
                            return result->toString(true);
                        }
                    }
                }
                return {};
            }
        }

        template<typename T>
        [[nodiscard]] bool compareCommonProperties(const Pattern &other) const {
            return typeid(other) == typeid(std::remove_cvref_t<T>) &&
                   this->m_offset == other.m_offset &&
                   this->m_size == other.m_size &&
                   (this->m_attributes == nullptr || other.m_attributes == nullptr || *this->m_attributes == *other.m_attributes) &&
                   (this->m_endian == other.m_endian || (!this->m_endian.has_value() && other.m_endian == std::endian::native) || (!other.m_endian.has_value() && this->m_endian == std::endian::native)) &&
                   *this->m_variableName == *other.m_variableName &&
                   *this->m_typeName == *other.m_typeName &&
                   this->m_section == other.m_section;
        }

    protected:
        std::unique_ptr<std::string> m_cachedDisplayValue;
        bool m_validDisplayValue = false;
        std::unique_ptr<std::vector<u8>> m_cachedBytes;

    private:
        friend pl::core::Evaluator;

        core::Evaluator *m_evaluator;

        std::unique_ptr<std::map<std::string, std::vector<core::Token::Literal>>> m_attributes;
        std::weak_ptr<Pattern> m_parent;
        u32 m_line = 0;

        std::set<std::string>::const_iterator m_variableName;
        pl::core::Location m_variableLocation;
        std::set<std::string>::const_iterator m_typeName;
        std::optional<u64> m_arrayIndex;

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
