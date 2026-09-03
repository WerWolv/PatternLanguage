#pragma once

#include <pl/patterns/pattern.hpp>

#include <pl/patterns/pattern_character.hpp>
#include <pl/core/errors/runtime_errors.hpp>

namespace pl::ptrn {

    class PatternString : public Pattern,
                          public IIndexable {
    public:
        PatternString(core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        [[nodiscard]] std::shared_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternString(*this));
        }

        [[nodiscard]] core::Token::Literal getValue() const override {
            return transformValue(this->getValue(this->getSize()));
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getEntries() override {
            return { };
        }

        [[nodiscard]] std::vector<std::shared_ptr<Pattern>> getSortedEntries() override {
            return { };
        }

        void setEntries(const std::vector<std::shared_ptr<Pattern>> &) override {

        }

        /**
         * @brief Gets this string's own encoding
         * @return This pattern's own [[encoding]] attribute, the evaluator's default
         * encoding if none, or empty when neither is set. An empty result falls back
         * to the codec's own default.
         */
        [[nodiscard]] std::string getEncodingName() const {
            if (const auto &arguments = this->getAttributeArguments("encoding"); !arguments.empty())
                return arguments[0].toString(true);
            return this->getEvaluator()->getDefaultEncoding();
        }

        std::string getValue(size_t size) const {
            if (size == 0)
                return "";

            auto *evaluator = this->getEvaluator();

            std::string buffer(size, '\x00');
            evaluator->readData(this->getOffset(), buffer.data(), size, this->getSection());

            if (const auto &codec = evaluator->getStringEncodeDecode(); codec != nullptr) {
                const auto encoding = this->getEncodingName();
                auto decoded = codec->decode({ reinterpret_cast<const u8*>(buffer.data()), buffer.size() }, encoding);
                if (decoded.bytesConsumed != buffer.size())
                    core::err::E0004.throwError(fmt::format("invalid byte sequence for encoding '{}'", encoding));

                return decoded.text;
            }

            return buffer;
        }

        std::vector<u8> getBytesOf(const core::Token::Literal &value) const override {
            if (auto stringValue = std::get_if<std::string>(&value); stringValue != nullptr) {
                std::vector<u8> bytes;

                if (const auto &codec = this->getEvaluator()->getStringEncodeDecode(); codec != nullptr) {
                    const auto encoding = this->getEncodingName();
                    auto encoded = codec->encode(*stringValue, encoding);
                    if (!encoded.has_value())
                        core::err::E0004.throwError(fmt::format("text has no byte value in encoding '{}'", encoding));

                    bytes = *encoded;
                } else {
                    bytes = { stringValue->begin(), stringValue->end() };
                }

                // This field owns a fixed number of bytes in the file. A longer write would
                // overwrite whatever comes right after it. Truncate or pad with NUL to the
                // field's own size, the same contract every other pattern type already has.
                bytes.resize(this->getSize());
                return bytes;
            } else
                return { };
        }

        /**
         * @brief Force-writes `value`, substituting a replacement character for
         * anything the pattern's encoding cannot represent
         * @param value Value to write
         * @note setValue() rejects such a value instead; an editor that offers an
         * explicit lossy override calls this one directly.
         */
        void setValueLossy(const std::string &value) {
            std::vector<u8> bytes;

            if (const auto &codec = this->getEvaluator()->getStringEncodeDecode(); codec != nullptr)
                bytes = codec->encodeLossy(value, this->getEncodingName());
            else
                bytes = { value.begin(), value.end() };

            bytes.resize(this->getSize());
            this->getEvaluator()->writeData(this->getOffset(), bytes.data(), bytes.size(), this->getSection());
            this->clearFormatCache();
            this->clearByteCache();
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String";
        }

        [[nodiscard]] std::string toString() override {
            auto value = this->getValue();
            auto result = value.toString(false);

            return Pattern::callUserFormatFunc(value, true).value_or(result);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string formatDisplayValue() override {
            auto *evaluator = this->getEvaluator();
            const auto fullSize = this->getSize();

            if (fullSize == 0)
                return "\"\"";

            constexpr size_t DisplayBudget = 0x7F;

            // UTF-32, the widest encoding decode() supports, spends 4 bytes per
            // code point. This margin holds DisplayBudget code points under any
            // of them.
            auto size = std::min<size_t>(fullSize, DisplayBudget * 4);

            std::string buffer(size, 0x00);
            evaluator->readData(this->getOffset(), buffer.data(), size, this->getSection());

            if (auto formatted = Pattern::callUserFormatFunc(buffer); formatted.has_value())
                return *formatted;

            const auto &codec = evaluator->getStringEncodeDecode();

            // No codec configured keeps the old raw-byte display, unescaped by any
            // encoding. A configured codec reports an undecodable buffer as invalid,
            // rather than substituting a replacement character into the display.
            if (codec == nullptr) {
                bool truncated = size < fullSize;

                if (buffer.size() > DisplayBudget) {
                    buffer.resize(DisplayBudget);
                    truncated = true;
                }

                const auto pos = buffer.find_last_not_of('\x00');
                if (pos == std::string::npos)
                    return "\"\"";
                buffer.erase(pos + 1);

                return fmt::format("\"{0}\" {1}", hlp::encodeByteString({ buffer.begin(), buffer.end() }), truncated ? "(truncated)" : "");
            }

            const auto encoding = this->getEncodingName();
            const auto decoded = codec->decode({ reinterpret_cast<const u8*>(buffer.data()), buffer.size() }, encoding, DisplayBudget);

            if (decoded.stopReason == core::DecodeStop::MalformedBytes)
                core::err::E0004.throwError(fmt::format("invalid byte sequence for encoding '{}'", encoding));

            const bool truncated = decoded.bytesConsumed < fullSize;
            return fmt::format("\"{0}\" {1}", decoded.text, truncated ? "(truncated)" : "");
        }

        std::shared_ptr<Pattern> getEntry(size_t index) const override {
            auto result = std::make_shared<PatternCharacter>(this->getEvaluator(), this->getOffset() + index, getLine());
            result->setVariableName(fmt::format("{}[{}]", this->getVariableName(), index));
            result->setSection(this->getSection());

            return result;
        }

        size_t getEntryCount() const override {
            return this->getSize();
        }

        void forEachEntryImpl(const std::vector<std::shared_ptr<Pattern>> &patterns, u64 start, u64 end, const std::function<void (u64, const std::shared_ptr<Pattern>&)> &callback) override {
            std::ignore = patterns;

            for (auto i = start; i < end; i++)
                callback(i, this->getEntry(i));
        }

        std::vector<u8> getRawBytes() override {
            std::vector<u8> result;

            this->forEachEntry(0, this->getEntryCount(), [&](u64, const auto &entry) {
                auto bytes = entry->getBytes();
                std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));
            });

            return result;
        }

    };

}