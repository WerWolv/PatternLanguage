#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::test {

    class TestPatternBitfields : public TestPattern {
    public:
        TestPatternBitfields() : TestPattern("Bitfields") {
            auto testBitfield = create<PatternBitfield>("TestBitfield", "testBitfield", 0x25, 0, 2 + 3 + (4 * 4));

            std::vector<std::shared_ptr<Pattern>> bitfieldFields;
            {
                bitfieldFields.push_back(create<PatternBitfieldField>("", "a", 0x25, 0, 2, testBitfield.get()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "b", 0x25, 2, 3, testBitfield.get()));

                auto nestedBitfield = create<PatternBitfield>("NestedBitfield", "c", 0x25, 5, 4 * 2);
                std::vector<std::shared_ptr<Pattern>> nestedFields;
                {
                    nestedFields.push_back(create<PatternBitfieldField>("", "nestedA", 0x25, 5, 4, testBitfield.get()));
                    nestedFields.push_back(create<PatternBitfieldField>("", "nestedB", 0x26, 1, 4, testBitfield.get()));
                }
                nestedBitfield->setParentBitfield(testBitfield.get());
                nestedBitfield->setFields(std::move(nestedFields));
                nestedBitfield->setEndian(std::endian::big);
                bitfieldFields.push_back(move(nestedBitfield));

                bitfieldFields.push_back(create<PatternBitfieldField>("", "d", 0x26, 5, 4, testBitfield.get()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "e", 0x27, 1, 4, testBitfield.get()));
            }

            testBitfield->setFields(std::move(bitfieldFields));
            testBitfield->setEndian(std::endian::big);

            addPattern(std::move(testBitfield));
        }
        ~TestPatternBitfields() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                bitfield NestedBitfield {
                    nestedA : 4;
                    nestedB : 4;
                };

                bitfield TestBitfield {
                    a : 2;
                    b : 3;
                    NestedBitfield c;
                    d : 4;
                    e : 4;
                };

                be TestBitfield testBitfield @ 0x25;

                std::assert(testBitfield.a == 0x01, "Field A invalid");
                std::assert(testBitfield.b == 0x01, "Field B invalid");
                std::assert(testBitfield.c.nestedA == 0x02, "Nested field A invalid");
                std::assert(testBitfield.c.nestedB == 0x08, "Nested field B invalid");
                std::assert(testBitfield.d == 0x08, "Field D invalid");
                std::assert(testBitfield.e == 0x08, "Field E invalid");
            )";
        }
    };

}