#pragma once

#include "test_pattern.hpp"

#include <pl/pattern_language.hpp>
#include <pl/core/string_encode_decode.hpp>

namespace pl::test {

    /**
     * @brief A predictable codec for the std::mem::read_string tests
     *
     * Under "latin1", a byte gives the code point of the same value. A byte above 0x7F
     * gives two UTF-8 bytes, which raw bytes cannot give. Under "ascii", a byte above
     * 0x7F is malformed. It counts its calls, so a test can show that it was not used.
     */
    class ReadStringCodec : public core::StringEncodeDecode {
    public:
        [[nodiscard]] core::DecodeResult decode(std::span<const u8> bytes, std::string_view encoding,
                std::optional<size_t>) const override {
            m_decodeCount += 1;

            core::DecodeResult result;
            for (const u8 byte : bytes) {
                if (byte >= 0x80 && encoding == "ascii") {
                    result.stopReason = core::DecodeStop::MalformedBytes;
                    return result;
                }

                if (byte < 0x80) {
                    result.text += char(byte);
                } else {
                    result.text += char(0xC0 | (byte >> 6));
                    result.text += char(0x80 | (byte & 0x3F));
                }

                result.bytesConsumed += 1;
                result.codepointCount += 1;
            }

            return result;
        }

        [[nodiscard]] std::optional<std::vector<u8>> encode(std::string_view text, std::string_view) const override {
            return std::vector<u8>(text.begin(), text.end());
        }

        [[nodiscard]] std::vector<u8> encodeLossy(std::string_view text, std::string_view) const override {
            return std::vector<u8>(text.begin(), text.end());
        }

        [[nodiscard]] size_t getDecodeCount() const { return m_decodeCount; }

    private:
        mutable size_t m_decodeCount = 0;
    };

    /**
     * @brief With no #pragma encoding, read_string returns the raw bytes and does not call the codec
     *
     * The test data starts with the PNG signature 89 50 4E 47. The 0x89 is not valid UTF-8.
     */
    class TestPatternReadStringNoPragma : public TestPattern {
    public:
        TestPatternReadStringNoPragma(core::Evaluator *evaluator) : TestPattern(evaluator, "ReadStringNoPragma") { }
        ~TestPatternReadStringNoPragma() override = default;

        void setup() override {
            m_codec = std::make_shared<ReadStringCodec>();
            m_runtime->setStringEncodeDecode(m_codec);
        }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                std::assert(builtin::std::mem::read_string(0, 4) == "\x89PNG", "no pragma did not give raw bytes");
                std::assert(builtin::std::mem::read_string(0, 4, 0, "") == "\x89PNG", "an empty encoding did not give raw bytes");
            )";
        }

        [[nodiscard]] bool runChecks(const std::vector<std::shared_ptr<ptrn::Pattern>> &) const override {
            return m_codec->getDecodeCount() == 0;
        }

    private:
        std::shared_ptr<ReadStringCodec> m_codec;
    };

    /**
     * @brief Under a non-UTF-8 #pragma encoding, read_string decodes, and "UTF-8" gives raw bytes
     */
    class TestPatternReadStringDecoded : public TestPattern {
    public:
        TestPatternReadStringDecoded(core::Evaluator *evaluator) : TestPattern(evaluator, "ReadStringDecoded") { }
        ~TestPatternReadStringDecoded() override = default;

        void setup() override {
            m_runtime->setStringEncodeDecode(std::make_shared<ReadStringCodec>());
        }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #pragma encoding latin1

                std::assert(builtin::std::mem::read_string(0, 4) == "\xC2\x89PNG", "the pragma's encoding was not used");
                std::assert(builtin::std::mem::read_string(0, 4, 0) == "\xC2\x89PNG", "the pragma's encoding was not used with a section");
                std::assert(builtin::std::mem::read_string(0, 4, 0, "") == "\xC2\x89PNG", "an empty encoding did not use the pragma's encoding");
                std::assert(builtin::std::mem::read_string(0, 4, 0, "UTF-8") == "\x89PNG", "UTF-8 did not give raw bytes");
                std::assert(builtin::std::mem::read_string(0, 0) == "", "an empty read did not give an empty string");
            )";
        }
    };

    /**
     * @brief With no codec set, read_string returns the raw bytes under any #pragma encoding
     */
    class TestPatternReadStringNoCodec : public TestPattern {
    public:
        TestPatternReadStringNoCodec(core::Evaluator *evaluator) : TestPattern(evaluator, "ReadStringNoCodec") { }
        ~TestPatternReadStringNoCodec() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #pragma encoding latin1

                std::assert(builtin::std::mem::read_string(0, 4) == "\x89PNG", "no codec did not give raw bytes");
            )";
        }
    };

    /**
     * @brief Bytes that do not decode under the encoding stop the pattern
     */
    class TestPatternReadStringUndecodableFail : public TestPattern {
    public:
        TestPatternReadStringUndecodableFail(core::Evaluator *evaluator) : TestPattern(evaluator, "ReadStringUndecodableFail", Mode::Failing) { }
        ~TestPatternReadStringUndecodableFail() override = default;

        void setup() override {
            m_runtime->setStringEncodeDecode(std::make_shared<ReadStringCodec>());
        }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #pragma encoding ascii

                str value = builtin::std::mem::read_string(0, 4);
            )";
        }
    };

}
