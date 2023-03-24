#include <pl/formatters/formatter.hpp>

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
            addLine(pattern->getVariableName());
            pushIndent();
            pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, auto member) {
                this->m_inArray = true;
                member->accept(*this);
            });
            popIndent();
        }

        void formatPointer(ptrn::PatternPointer *pattern) {
            addLine(pattern->getVariableName());
            pushIndent();
            pattern->getPointedAtPattern()->accept(*this);
            popIndent();
        }

        template<typename T>
        void formatObject(T *pattern) {
            if (pattern->isSealed()) {
                formatString(pattern);
            } else {
                addLine(pattern->getVariableName());
                pushIndent();

                for (const auto &[name, value] : this->getMetaInformation(pattern))
                    addLine(name, ::fmt::format("\"{}\"", value));

                pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, auto member) {
                    member->accept(*this);
                });
                popIndent();
            }
        }

        void formatString(pl::ptrn::Pattern *pattern) {
            auto result = pattern->toString();

            result = wolv::util::replaceStrings(result, "\n", " ");
            addLine(pattern->getVariableName(), ::fmt::format("\"{}\"", hlp::encodeByteString({ result.begin(), result.end() })));
        }

        void formatValue(pl::ptrn::Pattern *pattern) {
            auto result = pattern->toString();

            bool number = std::all_of(result.begin(), result.end(), [](char c) { return std::isdigit(c) || c == '.' || c == '-' || c == '+'; });
            bool needsEscape = std::any_of(result.begin(), result.end(), [](char c) { return std::ispunct(c) || !std::isprint(c); });

            if (!number && needsEscape)
                formatString(pattern);
            else
                addLine(pattern->getVariableName(), wolv::util::replaceStrings(result, "\n", " "));
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

        [[nodiscard]] std::string getFileExtension() const override { return ".yml"; }

        [[nodiscard]] std::vector<u8> format(const PatternLanguage &runtime) override {
            YamlPatternVisitor visitor;
            visitor.enableMetaInformation(this->isMetaInformationEnabled());

            for (const auto& pattern : runtime.getAllPatterns()) {
                pattern->accept(visitor);
            }

            auto result = "---\n" + visitor.getResult();
            return { result.begin(), result.end() };
        }
    };

}