#include <pl/formatters/formatter.hpp>

namespace pl::gen::fmt {

    class FormatterHtml : public Formatter {
    public:
        FormatterHtml() : Formatter("html") { }
        ~FormatterHtml() override = default;

        [[nodiscard]] std::string getFileExtension() const override { return ".html"; }

        [[nodiscard]] std::vector<u8> format(const PatternLanguage &runtime) override {
            auto result = generateHtml(runtime);

            return { result.begin(), result.end() };
        }

    private:
        static std::string generateTooltip(const std::vector<ptrn::Pattern*> &patterns) {
            if (patterns.empty())
                return "";

            std::string content;
            for (auto *pattern : patterns) {
                content += ::fmt::format(R"html({} {} | {}<br>)html", pattern->getFormattedName(), pattern->getVariableName(), pattern->toString());
            }

            if (!content.empty())
                content.resize(content.size() - 4);

            return ::fmt::format(R"html(<br><span class="pattern_language_tooltip" style="background-color: #{:08X}"><div class="pattern_language_tooltip_text">{}</div></span>)html", (hlp::changeEndianess(patterns.front()->getColor(), std::endian::big) | 0x000000FF) & 0xAFAFAFFF, content);
        }

        static std::string generateCell(u64 address, const PatternLanguage &runtime) {
            auto patterns = runtime.getPatternsAtAddress(address);

            u8 byte = 0;
            runtime.getInternals().evaluator->readData(address, &byte, sizeof(byte), ptrn::Pattern::MainSectionId);

            return ::fmt::format(R"html(<div class="pattern_language_cell" style="background-color: #{}">{:02X}{}</div>)html",
                                 patterns.empty() ? "transparent" : ::fmt::format("{:08X}", hlp::changeEndianess(patterns.front()->getColor(), std::endian::big)),
                                 byte,
                                 generateTooltip(patterns)
                                 );
        }

        static std::string generateRow(u64 address, const PatternLanguage &runtime) {
            std::string result;

            result += R"html(<div class="pattern_language_row">)html";
            result += ::fmt::format(R"html(<div class="pattern_language_address">{:08X}</div>)html", address);

            for (u64 i = address; i < runtime.getInternals().evaluator->getDataSize() && i < address + 0x10; i++) {
                result += generateCell(i, runtime);

                if ((i & 0x0F) == 0x07)
                    result += R"html(<div class="pattern_language_cell">&nbsp;</div>)html";
            }

            result += R"html(</div><br>)html";

            return result;
        }

        static std::string generateHtml(const PatternLanguage &runtime) {
            std::string rows;

            auto evaluator = runtime.getInternals().evaluator;

            for (u64 i = evaluator->getDataBaseAddress(); i < evaluator->getDataSize(); i += 0x10) {
                rows += generateRow(i, runtime);
            }

            return R"html(
<div>
    <style type="text/css">
        .pattern_language_container {
            display: inline-block;
        }

        .pattern_language_row {
            margin: 0px;
        }

        .pattern_language_address {
            float: left;
            padding-right: 10px;
            font-family: monospace;
        }

        .pattern_language_cell {
            float: left;
            padding-left: 1px;
            padding-right: 1px;
            font-family: monospace;
        }

        .pattern_language_tooltip_text {
            color: white;
            text-align: center;
        }

        .pattern_language_tooltip {
            visibility: hidden;
            border: solid 1px darkgray;

            padding: 5px 5px;

            position: absolute;
            z-index: 1;
            pointer-events : none
        }

        .pattern_language_cell:hover .pattern_language_tooltip {
            visibility: visible;
        }
    </style>

    <div class="pattern_language_container">
        <div class="pattern_language_row">
            <div class="pattern_language_address">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</div>
            <div class="pattern_language_cell">00</div>
            <div class="pattern_language_cell">01</div>
            <div class="pattern_language_cell">02</div>
            <div class="pattern_language_cell">03</div>
            <div class="pattern_language_cell">04</div>
            <div class="pattern_language_cell">05</div>
            <div class="pattern_language_cell">06</div>
            <div class="pattern_language_cell">07</div>
            <div class="pattern_language_cell">&nbsp;</div>
            <div class="pattern_language_cell">08</div>
            <div class="pattern_language_cell">09</div>
            <div class="pattern_language_cell">0A</div>
            <div class="pattern_language_cell">0B</div>
            <div class="pattern_language_cell">0C</div>
            <div class="pattern_language_cell">0D</div>
            <div class="pattern_language_cell">0E</div>
            <div class="pattern_language_cell">0F</div>
        </div>
        <br>
        )html" + rows + R"html(
    </div>
</div>
            )html";
        }
    };

}