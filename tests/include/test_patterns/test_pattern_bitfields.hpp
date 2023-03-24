#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::test {

    class TestPatternBitfields : public TestPattern {
    public:
        TestPatternBitfields() : TestPattern("Bitfields") {
            auto testBitfield = create<PatternBitfield>("TestBitfield", "testBitfield", 0x25, 0, 2 + 3 + (4 * 8));

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

                auto array = create<PatternBitfieldArray>("NestedBitfield", "f", 0x27, 5, 4 * 4);
                std::vector<std::shared_ptr<Pattern>> arrayEntries;
                {
                    auto array0Bitfield = create<PatternBitfield>("NestedBitfield", "[0]", 0x27, 5, 4 * 2);
                    std::vector<std::shared_ptr<Pattern>> array0Fields;
                    {
                        array0Fields.push_back(create<PatternBitfieldField>("", "nestedA", 0x27, 5, 4, array0Bitfield.get()));
                        array0Fields.push_back(create<PatternBitfieldField>("", "nestedB", 0x28, 1, 4, array0Bitfield.get()));
                    }
                    array0Bitfield->setParentBitfield(array.get());
                    array0Bitfield->setFields(std::move(array0Fields));
                    array0Bitfield->setEndian(std::endian::big);
                    arrayEntries.push_back(move(array0Bitfield));

                    auto array1Bitfield = create<PatternBitfield>("NestedBitfield", "[1]", 0x28, 5, 4 * 2);
                    std::vector<std::shared_ptr<Pattern>> array1Fields;
                    {
                        array1Fields.push_back(create<PatternBitfieldField>("", "nestedA", 0x28, 5, 4, array1Bitfield.get()));
                        array1Fields.push_back(create<PatternBitfieldField>("", "nestedB", 0x29, 1, 4, array1Bitfield.get()));
                    }
                    array1Bitfield->setParentBitfield(array.get());
                    array1Bitfield->setFields(std::move(array1Fields));
                    array1Bitfield->setEndian(std::endian::big);
                    arrayEntries.push_back(move(array1Bitfield));
                }
                array->setParentBitfield(testBitfield.get());
                array->setEntries(std::move(arrayEntries));
                array->setEndian(std::endian::big);
                bitfieldFields.push_back(move(array));
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
                    NestedBitfield f[c.nestedA];
                };

                be TestBitfield testBitfield @ 0x25;

                std::assert(testBitfield.a == 0x01, "Field A invalid");
                std::assert(testBitfield.b == 0x01, "Field B invalid");
                std::assert(testBitfield.c.nestedA == 0x02, "Nested field A invalid");
                std::assert(testBitfield.c.nestedB == 0x08, "Nested field B invalid");
                std::assert(testBitfield.d == 0x08, "Field D invalid");
                std::assert(testBitfield.e == 0x08, "Field E invalid");
                std::assert(testBitfield.f[0].nestedA == 0x02, "Nested array[0] field A invalid");
                std::assert(testBitfield.f[0].nestedB == 0x0A, "Nested array[0] field B invalid");
                std::assert(testBitfield.f[1].nestedA == 0x08, "Nested array[1] field A invalid");
                std::assert(testBitfield.f[1].nestedB == 0x0F, "Nested array[1] field B invalid");
            )";
        }
    };

}