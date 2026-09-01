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
                if (!decoded.has_value())
                    core::err::E0004.throwError(fmt::format("invalid byte sequence for encoding '{}'", encoding));

                return *decoded;
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

            // Read a little past DisplayBudget, so a multi-byte codepoint at the
            // cutoff usually has the bytes to decode whole. The decode below still
            // backs off further on failure; this only keeps that the common case.
            constexpr size_t DisplayBudget = 0x7F;
            auto size = std::min<size_t>(fullSize, DisplayBudget + 8);

            if (size == 0)
                return "\"\"";

            std::string buffer(size, 0x00);
            evaluator->readData(this->getOffset(), buffer.data(), size, this->getSection());

            if (auto formatted = Pattern::callUserFormatFunc(buffer); formatted.has_value())
                return *formatted;

            const auto &codec = evaluator->getStringEncodeDecode();
            bool truncated = size < fullSize;

            // No codec configured keeps the old raw-byte display, unescaped by any
            // encoding. A configured codec reports an undecodable buffer as invalid,
            // rather than substituting a replacement character into the display.
            if (codec == nullptr) {
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

            // The read above can itself end mid-codepoint. decode() fails on the whole
            // buffer in that case, not just its incomplete tail, so back off a few
            // bytes and retry rather than reporting a merely-truncated read as invalid.
            std::optional<std::string> decoded;
            for (size_t backoff = 0; backoff <= std::min<size_t>(4, buffer.size()); ++backoff) {
                decoded = codec->decode({ reinterpret_cast<const u8*>(buffer.data()), buffer.size() - backoff }, encoding);
                if (decoded.has_value()) {
                    if (backoff > 0)
                        truncated = true;
                    break;
                }
            }
            if (!decoded.has_value())
                core::err::E0004.throwError(fmt::format("invalid byte sequence for encoding '{}'", encoding));

            // Trim the decoded text itself, not the raw bytes, so a multi-byte
            // codepoint at the cutoff stays whole instead of splitting mid-sequence.
            if (decoded->size() > DisplayBudget) {
                auto cut = DisplayBudget;
                while (cut > 0 && (static_cast<u8>((*decoded)[cut]) & 0xC0) == 0x80)
                    --cut;
                decoded->resize(cut);
                truncated = true;
            }

            return fmt::format("\"{0}\" {1}", *decoded, truncated ? "(truncated)" : "");
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