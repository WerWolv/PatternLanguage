#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_bitfield.hpp>

namespace pl::test {

    class TestPatternBitfields : public TestPattern {
    public:
        TestPatternBitfields(core::Evaluator *evaluator) : TestPattern(evaluator, "Bitfields") {
            auto testBitfield = create<PatternBitfield>("TestBitfield", "testBitfield", 0x25, 0, 2 + 3 + (4 * 8), 0);

            std::vector<std::shared_ptr<Pattern>> bitfieldFields;
            {
                bitfieldFields.push_back(create<PatternBitfieldField>("", "a", 0x25, 0, 2, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "b", 0x25, 2, 3, 0, testBitfield->reference()));

                auto nestedBitfield = create<PatternBitfield>("NestedBitfield", "c", 0x25, 5, 4 * 2, 0);
                std::vector<std::shared_ptr<Pattern>> nestedFields;
                {
                    nestedFields.push_back(create<PatternBitfieldField>("", "nestedA", 0x25, 5, 4, 0, testBitfield->reference()));
                    nestedFields.push_back(create<PatternBitfieldField>("", "nestedB", 0x26, 1, 4, 0, testBitfield->reference()));
                }
                nestedBitfield->setParent(testBitfield->reference());
                nestedBitfield->setFields(std::move(nestedFields));
                nestedBitfield->setEndian(std::endian::big);
                bitfieldFields.push_back(std::move(nestedBitfield));

                bitfieldFields.push_back(create<PatternBitfieldField>("", "d", 0x26, 5, 4, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "e", 0x27, 1, 4, 0, testBitfield->reference()));

                auto array = create<PatternBitfieldArray>("NestedBitfield", "f", 0x27, 5, 4 * 4, 0);
                std::vector<std::shared_ptr<Pattern>> arrayEntries;
                {
                    auto array0Bitfield = create<PatternBitfield>("NestedBitfield", "[0]", 0x27, 5, 4 * 2, 0);
                    std::vector<std::shared_ptr<Pattern>> array0Fields;
                    {
                        array0Fields.push_back(create<PatternBitfieldField>("", "nestedA", 0x27, 5, 4, 0, array0Bitfield->reference()));
                        array0Fields.push_back(create<PatternBitfieldField>("", "nestedB", 0x28, 1, 4, 0, array0Bitfield->reference()));
                    }
                    array0Bitfield->setParent(array->reference());
                    array0Bitfield->setFields(std::move(array0Fields));
                    array0Bitfield->setEndian(std::endian::big);
                    arrayEntries.push_back(std::move(array0Bitfield));

                    auto array1Bitfield = create<PatternBitfield>("NestedBitfield", "[1]", 0x28, 5, 4 * 2, 0);
                    std::vector<std::shared_ptr<Pattern>> array1Fields;
                    {
                        array1Fields.push_back(create<PatternBitfieldField>("", "nestedA", 0x28, 5, 4, 0, array1Bitfield->reference()));
                        array1Fields.push_back(create<PatternBitfieldField>("", "nestedB", 0x29, 1, 4, 0, array1Bitfield->reference()));
                    }
                    array1Bitfield->setParent(array->reference());
                    array1Bitfield->setFields(std::move(array1Fields));
                    array1Bitfield->setEndian(std::endian::big);
                    arrayEntries.push_back(std::move(array1Bitfield));
                }
                array->setParent(testBitfield->reference());
                array->setEntries(arrayEntries);
                array->setEndian(std::endian::big);
                bitfieldFields.push_back(std::move(array));
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
                    unsigned a : 2;
                    b : 3;
                    NestedBitfield c;
                    d : 4;
                    signed e : 4;
                    NestedBitfield f[c.nestedA];
                };

                be TestBitfield testBitfield @ 0x25;

                std::assert(testBitfield.a == 0x01, "Field A invalid");
                std::assert(testBitfield.b == 0x01, "Field B invalid");
                std::assert(testBitfield.c.nestedA == 0x02, "Nested field A invalid");
                std::assert(testBitfield.c.nestedB == 0x08, "Nested field B invalid");
                std::assert(testBitfield.d == 0x08, "Field D invalid");
                std::assert(testBitfield.e == -8, "Field E invalid");
                std::assert(testBitfield.f[0].nestedA == 0x02, "Nested array[0] field A invalid");
                std::assert(testBitfield.f[0].nestedB == 0x0A, "Nested array[0] field B invalid");
                std::assert(testBitfield.f[1].nestedA == 0x08, "Nested array[1] field A invalid");
                std::assert(testBitfield.f[1].nestedB == 0x0F, "Nested array[1] field B invalid");
            )";
        }
    };

    class TestPatternReversedBitfields : public TestPattern {
    public:
        TestPatternReversedBitfields(core::Evaluator *evaluator) : TestPattern(evaluator, "ReversedBitfields") {
            auto testBitfield = create<PatternBitfield>("Test", "test", 0x02, 0, 16, 0);

            std::vector<std::shared_ptr<Pattern>> bitfieldFields;
            {
                bitfieldFields.push_back(create<PatternBitfieldField>("", "flag0", 0x03, 7, 1, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "flag1", 0x03, 6, 1, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "flag2", 0x03, 5, 1, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "flag3", 0x03, 4, 1, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "flag4", 0x03, 3, 1, 0, testBitfield->reference()));
                bitfieldFields.push_back(create<PatternBitfieldField>("", "flag5", 0x03, 2, 1, 0, testBitfield->reference()));

                auto enumField = create<PatternBitfieldFieldEnum>("Enum", "enumerated", 0x02, 4, 6, 0, testBitfield->reference());
                std::map<std::string, PatternEnum::EnumValue> values;
                {
                    values.insert({ "A", { 0x39, 0x39 } });
                }
                enumField->setEnumValues(std::move(values));
                bitfieldFields.push_back(std::move(enumField));
            }

            testBitfield->setFields(std::move(bitfieldFields));
            testBitfield->setEndian(std::endian::big);
            testBitfield->setReversed(true);
            testBitfield->addAttribute("bitfield_order", { 1, 16 });

            addPattern(std::move(testBitfield));
        }
        ~TestPatternReversedBitfields() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #pragma endian big

                enum Enum : u8 {
                    A = 0x39,
                };

                bitfield Test {
                    bool flag0 : 1;
                    bool flag1 : 1;
                    bool flag2 : 1;
                    bool flag3 : 1;
                    bool flag4 : 1;
                    bool flag5 : 1;
                    Enum enumerated : 6;
                } [[bitfield_order(1, 16)]];

                Test test @ 2;

                std::assert(test.flag0 == true, "flag0 was invalid");
                std::assert(test.flag1 == true, "flag1 was invalid");
                std::assert(test.flag2 == true, "flag2 was invalid");
                std::assert(test.flag3 == false, "flag3 was invalid");
                std::assert(test.flag4 == false, "flag4 was invalid");
                std::assert(test.flag5 == false, "flag5 was invalid");
                std::assert(test.enumerated == Enum::A, "enumerated was invalid");
            )";
        }
    };

}