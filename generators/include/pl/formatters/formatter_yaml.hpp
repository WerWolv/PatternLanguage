#pragma once

#include <pl/formatters/formatter.hpp>

#include <wolv/utils/string.hpp>

namespace pl::gen::fmt {

    class YamlPatternVisitor : public FormatterPatternVisitor {
    public:
        YamlPatternVisitor() = default;

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
        void visit(pl::ptrn::Pattern& pattern)              override { formatString(&pattern);      }
        void visit(pl::ptrn::PatternError& pattern)         override { formatString(&pattern);      }

        [[nodiscard]] auto getResult() const {
            return this->m_result;
        }

        void pushIndent(u8 indent = 4) {
            this->m_indent += indent;
        }

        void popIndent(u8 indent = 4) {
            this->m_indent -= indent;
        }

    private:
        void addLine(const std::string &variableName, const std::string &str = "", bool addDash = false) {
            this->m_result += std::string(this->m_indent, ' ');
            if (addDash || this->m_inArray) {
                this->m_result += "- ";
                this->m_inArray = false;
            } else {
                this->m_result += ::fmt::format("{}: ", variableName);
            }
            this->m_result += str + "\n";
        }

        template<typename T>
        void formatArray(T *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            addLine(pattern->getVariableName());
            pushIndent();
            pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, const auto &member) {
                this->m_inArray = true;
                member->accept(*this);
            });
            popIndent();
        }

        void formatPointer(ptrn::PatternPointer *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            addLine(pattern->getVariableName());
            pushIndent();
            pattern->getPointedAtPattern()->accept(*this);
            popIndent();
        }

        template<typename T>
        void formatObject(T *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            if (pattern->isSealed()) {
                formatValue(pattern);
            } else {
                addLine(pattern->getVariableName());
                pushIndent();

                for (const auto &[name, value] : this->getMetaInformation(pattern))
                    addLine(name, ::fmt::format("\"{}\"", value));

                pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, const auto &member) {
                    member->accept(*this);
                });
                popIndent();
            }
        }

        // Writes one code point as a YAML numeric escape. Section 5.7 gives
        // three forms. This uses the shortest one that fits.
        static std::string escapeCodepoint(u32 codepoint) {
            if (codepoint <= 0xFF)
                return ::fmt::format("\\x{:02X}", codepoint);
            if (codepoint <= 0xFFFF)
                return ::fmt::format("\\u{:04X}", codepoint);
            return ::fmt::format("\\U{:08X}", codepoint);
        }

        // Escapes text for a YAML double quoted scalar. See YAML 1.2.1 chapter 5.
        //
        // toString() decodes the string first, so this gets UTF-8 text. Byte
        // escapes would hide that text. A reader also reads \xNN as code point
        // NN, not as byte NN.
        //
        // Writes each character as itself, unless the spec does not permit it:
        //   5.1  No C0 control but tab, LF and CR. No DEL. No C1 control but
        //        NEL. No surrogate. Not U+FFFE or U+FFFF.
        //   5.2  A scalar holds nb-json only: #x9 | [#x20-#x10FFFF].
        //   5.2  Escape a byte order mark in content.
        //   5.7  Escape the quote and the backslash.
        //
        // U+2028 and U+2029 stay as they are. Section 5.4 makes them normal
        // characters in YAML 1.2. NEL is normal too, but a YAML 1.1 reader
        // folds it to a space, so it keeps the \N escape.
        //
        // An invalid byte has no code point. It becomes \xNN, which a reader
        // reads as a Latin-1 character. Only the !!binary tag can hold a byte,
        // and that changes the document.
        static std::string escapeYamlString(std::string_view text) {
            std::string result;

            for (size_t offset = 0; offset < text.size();) {
                const auto [codepoint, length] = hlp::decodeUtf8Codepoint(text.substr(offset));

                if (length == 0) {
                    result += escapeCodepoint(u8(text[offset]));
                    offset += 1;
                    continue;
                }

                switch (codepoint) {
                    case U'"':   result += "\\\""; break;
                    case U'\\':  result += "\\\\"; break;
                    case U'\t':  result += "\\t";  break;
                    case U'\n':  result += "\\n";  break;
                    case U'\r':  result += "\\r";  break;
                    case 0x0085: result += "\\N";  break;  // next line
                    default: {
                        const bool controlCode  = codepoint < 0x20 || codepoint == 0x7F
                                               || (codepoint >= 0x80 && codepoint <= 0x9F);
                        const bool nonCharacter = codepoint == 0xFFFE || codepoint == 0xFFFF;
                        const bool byteOrderMark = codepoint == 0xFEFF;

                        if (controlCode || nonCharacter || byteOrderMark)
                            result += escapeCodepoint(codepoint);
                        else
                            result += text.substr(offset, length);
                        break;
                    }
                }

                offset += length;
            }

            return result;
        }

        void formatString(pl::ptrn::Pattern *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            // Escape a line break. Do not replace it with a space, which loses
            // data.
            addLine(pattern->getVariableName(), ::fmt::format("\"{}\"", escapeYamlString(pattern->toString())));
        }

        std::string formatLiteral(const core::Token::Literal &literal) {
            auto result = std::visit(wolv::util::overloaded {
                [&](integral auto value)            -> std::string { return ::fmt::format("{}", value); },
                [&](std::floating_point auto value) -> std::string { return ::fmt::format("{}", value); },
                [&](const std::string &value)       -> std::string { return ::fmt::format("\"{}\"", value); },
                [&](bool value)                     -> std::string { return value ? "true" : "false"; },
                [&](char value)                     -> std::string { return ::fmt::format("\"{}\"", value); },
                [&](const std::shared_ptr<ptrn::Pattern> &value) -> std::string { return ::fmt::format("\"{}\"", value->toString()); },
            }, literal);

            const bool number = std::ranges::all_of(result, [](char c) { return std::isdigit(c) || c == '.' || c == '-' || c == '+'; });
            const bool needsEscape = std::ranges::any_of(result, [](char c) { return std::ispunct(c) || !std::isprint(c); });

            if (!number && needsEscape)
                return result;
            else
                return wolv::util::replaceStrings(result, "\n", " ");
        }

        void formatValue(pl::ptrn::Pattern *pattern) {
            if (pattern->getVisibility() == ptrn::Visibility::Hidden) return;
            if (pattern->getVisibility() == ptrn::Visibility::TreeHidden) return;

            if (!pattern->getReadFormatterFunction().empty())
                formatString(pattern);
            else {
                try {
                    auto literal = pattern->getValue();

                    addLine(pattern->getVariableName(), wolv::util::replaceStrings(formatLiteral(literal), "\n", " "));
                } catch (std::exception &e) {
                    addLine(pattern->getVariableName(), ::fmt::format("\"<Error: {}>\"", wolv::util::replaceStrings(e.what(), "\n", " ")));
                }
            }
        }

    private:
        bool m_inArray = false;
        std::string m_result;
        u32 m_indent = 0;
    };

    class FormatterYaml : public Formatter {
    public:
        FormatterYaml() : Formatter("yaml") { }
        ~FormatterYaml() override = default;

        [[nodiscard]] std::string getFileExtension() const override { return "yml"; }

        [[nodiscard]] std::vector<u8> format(const PatternLanguage &runtime) override {
            YamlPatternVisitor visitor;
            visitor.enableMetaInformation(this->isMetaInformationEnabled());

            for (const auto& pattern : runtime.getPatterns()) {
                pattern->accept(visitor);
            }

            auto result = "---\n" + visitor.getResult();
            return { result.begin(), result.end() };
        }
    };

}