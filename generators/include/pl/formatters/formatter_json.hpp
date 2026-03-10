#pragma once

#include <pl/formatters/formatter.hpp>
#include <pl/patterns/pattern_error.hpp>

namespace pl::gen::fmt {

    class JsonPatternVisitor : public FormatterPatternVisitor {
    public:
        JsonPatternVisitor() = default;
        ~JsonPatternVisitor() override = default;

        void visit(pl::ptrn::PatternArrayDynamic& pattern)  override { formatArray(&pattern);       }
        void visit(pl::ptrn::PatternArrayStatic& pattern)   override { formatArray(&pattern);       }
        void visit(pl::ptrn::PatternBitfieldField& pattern) override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternBitfieldArray& pattern) override { formatArray(&pattern);       }
        void visit(pl::ptrn::PatternBitfield& pattern)      override { formatObject(&pattern);      }
        void visit(pl::ptrn::PatternBoolean& pattern)       override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternCharacter& pattern)     override { formatString(&pattern);      }
        void visit(pl::ptrn::PatternEnum& pattern)          override { formatString(&pattern);      }
        void visit(pl::ptrn::PatternFloat& pattern)         override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternPadding& pattern)       override { wolv::util::unused(pattern); }
        void visit(pl::ptrn::PatternPointer& pattern)       override { formatPointer(&pattern);     }
        void visit(pl::ptrn::PatternSigned& pattern)        override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternString& pattern)        override { formatString(&pattern);      }
        void visit(pl::ptrn::PatternStruct& pattern)        override { formatObject(&pattern);      }
        void visit(pl::ptrn::PatternUnion& pattern)         override { formatObject(&pattern);      }
        void visit(pl::ptrn::PatternUnsigned& pattern)      override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternWideCharacter& pattern) override { formatString(&pattern);      }
        void visit(pl::ptrn::PatternWideString& pattern)    override { formatString(&pattern);      }
        void visit(pl::ptrn::PatternError& pattern)         override { formatString(&pattern);      }
        void visit(pl::ptrn::Pattern& pattern)              override { formatString(&pattern);      }

        [[nodiscard]] auto getResult() const {
            return this->m_result;
        }

        void pushIndent() {
            this->m_indent += 4;
        }

        void popIndent() {
            this->m_indent -= 4;

            if (this->m_result.size() >= 2 && this->m_result.substr(this->m_result.size() - 2) == ",\n") {
                this->m_result.pop_back();
                this->m_result.pop_back();
                this->m_result.push_back('\n');
            }
        }

    private:
        void addLine(const std::string &variableName, const std::string& str, bool noVariableName = false) {
            this->m_result += std::string(this->m_indent, ' ');
            if (!noVariableName && !this->m_inArray)
                this->m_result += ::fmt::format("\"{}\": ", variableName);

            this->m_result  += str + "\n";

            this->m_inArray = false;
        }

        void formatString(pl::ptrn::Pattern *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            const auto rawBytes = pattern->toString();

            std::string result;
            for (size_t i = 0; i < rawBytes.size(); ) {
            const u8 ch = static_cast<u8>(rawBytes[i]);

            // Determine UTF-8 sequence length
            size_t seqLen = 0;
            u32 codepoint = 0;
            if (ch < 0x80) {
                seqLen = 1;
                codepoint = ch;
            } else if ((ch & 0xE0) == 0xC0 && ch >= 0xC2) {
                seqLen = 2;
                codepoint = ch & 0x1F;
            } else if ((ch & 0xF0) == 0xE0) {
                seqLen = 3;
                codepoint = ch & 0x0F;
            } else if ((ch & 0xF8) == 0xF0 && ch <= 0xF4) {
                seqLen = 4;
                codepoint = ch & 0x07;
            }

            // Validate continuation bytes
            bool valid = seqLen > 0;
            for (size_t j = 1; j < seqLen && valid; ++j) {
                const u8 cb = static_cast<u8>(rawBytes[i + j]);
                if (i + j >= rawBytes.size() || (cb & 0xC0) != 0x80) {
                    valid = false;
                } else {
                    codepoint = (codepoint << 6) | (cb & 0x3F);
                }
            }

            // Reject overlong sequences and surrogates
            if (valid) {
                if (seqLen == 2 && codepoint < 0x80)            valid = false;
                if (seqLen == 3 && codepoint < 0x800)           valid = false;
                if (seqLen == 4 && codepoint < 0x10000)         valid = false;
                if (codepoint >= 0xD800 && codepoint <= 0xDFFF) valid = false;
                if (codepoint > 0x10FFFF)                       valid = false;
            }

            if (!valid) {
                // Escape the single invalid byte
                result += ::fmt::format("\\u{:04x}", static_cast<u32>(ch));
                i += 1;
                continue;
            }

            // Emit the codepoint
            switch (codepoint) {
                case U'"':  result += "\\\""; break;
                case U'\\': result += "\\\\"; break;
                case U'\b': result += "\\b";  break;
                case U'\f': result += "\\f";  break;
                case U'\n': result += "\\n";  break;
                case U'\r': result += "\\r";  break;
                case U'\t': result += "\\t";  break;
                default:
                    if (codepoint < 0x20 || codepoint == 0x7F) {
                        result += ::fmt::format("\\u{:04x}", codepoint);
                    } else if (codepoint <= 0xFFFF) {
                        // Append the original UTF-8 bytes directly
                        for (size_t j = 0; j < seqLen; ++j)
                            result += rawBytes[i + j];
                    } else {
                        u32 code = codepoint - 0x10000;
                        u16 highSurrogate = 0xD800 + ((code >> 10) & 0x3FF);
                        u16 lowSurrogate  = 0xDC00 + (code & 0x3FF);
                        result += ::fmt::format("\\u{:04x}\\u{:04x}", highSurrogate, lowSurrogate);
                    }
                    break;
            }
            i += seqLen;
        }

            addLine(pattern->getVariableName(), ::fmt::format("\"{}\",", result));
        }

        template<typename T>
        void formatArray(T *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            addLine(pattern->getVariableName(), "[");
            pushIndent();
            pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, const auto &member) {
                this->m_inArray = true;
                member->accept(*this);
            });
            popIndent();
            addLine("", "],", true);
        }

        void formatPointer(ptrn::PatternPointer *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            addLine(pattern->getVariableName(), "{");
            pushIndent();
            pattern->getPointedAtPattern()->accept(*this);
            popIndent();
            addLine("", "},", true);
        }

        template<typename T>
        void formatObject(T *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            if (pattern->isSealed()) {
                formatValue(pattern);
            } else {
                addLine(pattern->getVariableName(), "{");
                pushIndent();

                for (const auto &[name, value] : this->getMetaInformation(pattern))
                    addLine(name, ::fmt::format("\"{}\",", value));

                pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, const auto &member) {
                    member->accept(*this);
                });
                popIndent();
                addLine("", "},", true);
            }
        }

        std::string formatLiteral(const core::Token::Literal &literal) {
            return std::visit(wolv::util::overloaded {
                [&](integral auto value)            -> std::string { return ::fmt::format("{}", value); },
                [&](std::floating_point auto value) -> std::string { return ::fmt::format("{}", value); },
                [&](const std::string &value)       -> std::string { return ::fmt::format("\"{}\"", value); },
                [&](bool value)                     -> std::string { return value ? "true" : "false"; },
                [&](char value)                     -> std::string { return ::fmt::format("\"{}\"", value); },
                [&](const std::shared_ptr<ptrn::Pattern> &value) -> std::string { return ::fmt::format("\"{}\"", value->toString()); },
            }, literal);
        }

        void formatValue(pl::ptrn::Pattern *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            if (!pattern->getReadFormatterFunction().empty())
                formatString(pattern);
            else {
                try {
                    auto literal = pattern->getValue();

                    addLine(pattern->getVariableName(), formatLiteral(literal) + ",");
                } catch (const std::exception &e) {
                    addLine(pattern->getVariableName(), ::fmt::format("\"<error: {}>\",", e.what()));
                }
            }
        }

    private:
        bool m_inArray = false;
        std::string m_result;
        u32 m_indent = 0;
    };

    class FormatterJson : public Formatter {
    public:
        FormatterJson() : Formatter("json") { }
        ~FormatterJson() override = default;

        [[nodiscard]] std::string getFileExtension() const override { return "json"; }

        [[nodiscard]] std::vector<u8> format(const PatternLanguage &runtime) override {
            JsonPatternVisitor visitor;
            visitor.enableMetaInformation(this->isMetaInformationEnabled());

            visitor.pushIndent();
            for (const auto& pattern : runtime.getPatterns()) {
                pattern->accept(visitor);
            }
            visitor.popIndent();

            auto result = visitor.getResult();
            result = ::fmt::format("{{\n{}}}", result);

            return { result.begin(), result.end() };
        }
    };

}
