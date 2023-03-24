#include <pl/formatters/formatter.hpp>

namespace pl::gen::fmt {

    class JsonPatternVisitor : public FormatterPatternVisitor {
    public:
        JsonPatternVisitor() = default;

        void visit(pl::ptrn::PatternArrayDynamic& pattern)  override { formatArray(&pattern);       }
        void visit(pl::ptrn::PatternArrayStatic& pattern)   override { formatArray(&pattern);       }
        void visit(pl::ptrn::PatternBitfieldField& pattern) override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternBitfieldArray& pattern) override { formatArray(&pattern);       }
        void visit(pl::ptrn::PatternBitfield& pattern)      override { formatObject(&pattern);      }
        void visit(pl::ptrn::PatternBoolean& pattern)       override { formatValue(&pattern);       }
        void visit(pl::ptrn::PatternCharacter& pattern)     override { formatValue(&pattern);       }
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
            auto result = pattern->toString();
            result = wolv::util::replaceStrings(result, "\n", " ");
            result = hlp::encodeByteString({ result.begin(), result.end() });

            addLine(pattern->getVariableName(), ::fmt::format("\"{}\",", result));
        }

        template<typename T>
        void formatArray(T *pattern) {
            addLine(pattern->getVariableName(), "[");
            pushIndent();
            pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, auto member) {
                this->m_inArray = true;
                member->accept(*this);
            });
            popIndent();
            addLine("", "],", true);
        }

        void formatPointer(ptrn::PatternPointer *pattern) {
            addLine(pattern->getVariableName(), "{");
            pushIndent();
            pattern->getPointedAtPattern()->accept(*this);
            popIndent();
            addLine("", "},", true);
        }

        template<typename T>
        void formatObject(T *pattern) {
            if (pattern->isSealed()) {
                formatValue(pattern);
            } else {
                addLine(pattern->getVariableName(), "{");
                pushIndent();

                for (const auto &[name, value] : this->getMetaInformation(pattern))
                    addLine(name, ::fmt::format("\"{}\",", value));

                pattern->forEachEntry(0, pattern->getEntryCount(), [&](u64, auto member) {
                    member->accept(*this);
                });
                popIndent();
                addLine("", "},", true);
            }
        }

        void formatValue(pl::ptrn::Pattern *pattern) {
            if (auto functionName = pattern->getReadFormatterFunction(); !functionName.empty())
                formatString(pattern);
            else
                addLine(pattern->getVariableName(), ::fmt::format("{},", pattern->toString()));
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

        [[nodiscard]] std::string getFileExtension() const override { return ".json"; }

        [[nodiscard]] std::vector<u8> format(const PatternLanguage &runtime) override {
            JsonPatternVisitor visitor;
            visitor.enableMetaInformation(this->isMetaInformationEnabled());

            visitor.pushIndent();
            for (const auto& pattern : runtime.getAllPatterns()) {
                pattern->accept(visitor);
            }
            visitor.popIndent();

            auto result = visitor.getResult();
            result = ::fmt::format("{{\n{}}}", result);

            return { result.begin(), result.end() };
        }
    };

}