#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternDollar : public TestPattern {
    public:
        TestPatternDollar(core::Evaluator *evaluator) : TestPattern(evaluator, "Dollar") {
        }
        ~TestPatternDollar() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                std::assert($[0x00] == 0x89, "Dollar offset read failed 1");
                std::assert($[0x05] == 0x0A, "Dollar offset read failed 2");
                std::assert($[0x13+$] == 0x34, "Dollar offset read failed 3");
                
                std::assert(addressof($) == 0, "Address of dollar operator failed 1");
                std::assert(sizeof($) == 0x000291E3, "Sizeof of dollar operator failed 1");
                
                auto section = builtin::std::mem::create_section("data");
                builtin::std::mem::copy_value_to_section("01234567890", section, 0);
                
                struct CurrentSectionRead {
                    u8 field1 = $[0];
                    u8 field2 = $[$-$+1];
                    $ += 3;
                    u8 field3 = $[$+$];
                    u8 field4 = $[$-1];
                    
                    $ = 7;
                    u8 field5 = $[$];
                    $ -= 100;
                    std::assert($ == 0xFFFFFFFFFFFFFFA3, "$ offset did not overflow");
                    u8 field6 = $[$+102];
                    
                    std::assert(addressof($) == 0, "Address of dollar operator failed 2");
                    std::assert(sizeof($) == 11, "Sizeof of dollar operator failed 2");
                };
                CurrentSectionRead currentSectionRead @ 0x00 in section;
                std::assert(currentSectionRead.field1 == 0x30, "Dollar offset operator did not read from current section 1");
                std::assert(currentSectionRead.field2 == 0x31, "Dollar offset operator did not read from current section 2");
                std::assert(currentSectionRead.field3 == 0x36, "Dollar offset operator did not read from current section 3");
                std::assert(currentSectionRead.field4 == 0x32, "Dollar offset operator did not read from current section 4");
                std::assert(currentSectionRead.field5 == 0x37, "Dollar offset operator did not read from current section 5");
                std::assert(currentSectionRead.field6 == 0x39, "Dollar offset operator did not read from current section 5");
                
                struct CurrentSectionRead2 {
                    u8 field1 = $[$];
                    u8 field2 = $[$-1];
                    
                    std::assert(addressof($) == 0, "Address of dollar operator failed 3");
                    std::assert(sizeof($) == 11, "Sizeof of dollar operator failed 3");
                };
                CurrentSectionRead2 currentSectionRead2 @ 0x03 in section;
                std::assert(currentSectionRead2.field1 == 0x33, "Dollar offset operator did not read with zero offset from current section 1");
                std::assert(currentSectionRead2.field2 == 0x32, "Dollar offset operator did not read with negative offset from current section 2");
                
                struct NegativeOffset {
                    std::assert($ == 6, "$ offset should be 6");
                };
                NegativeOffset negativeOffset1 @ -0xFFFFFFFFFFFFFFFA;
                NegativeOffset negativeOffset2 @ -0xFFFFFFFFFFFFFFFA in section;
                
                struct ReadArray {
                    u8 array[while($ == addressof(this) || $[$-1] != 0x36)];
                };
                ReadArray readArray1 @ 0x10;
                std::assert(builtin::std::core::member_count(readArray1.array) == 69, "Incorrect array length");
                std::assert(sizeof(readArray1.array) == 69, "Incorrect array size in bytes");
                std::assert(readArray1.array[15] == 87, "Incorrect array element value");
                
                ReadArray readArray2 @ 0x0 in section;
                std::assert(builtin::std::core::member_count(readArray2.array) == 7, "Incorrect array length from section");
                std::assert(sizeof(readArray2.array) == 7, "Incorrect array size in bytes from section");
                std::assert(readArray2.array[4] == 0x34, "Incorrect array element value from section");
                
                ReadArray readArray3 @ 0x4 in section;
                std::assert(builtin::std::core::member_count(readArray3.array) == 3, "Incorrect array length from section with offset 1");
                std::assert(sizeof(readArray3.array) == 3, "Incorrect array size in bytes from section with offset 1");
                std::assert(readArray3.array[2] == 0x36, "Incorrect array element value from section with offset 1");
                
                ReadArray readArray4 @ 0x6 in section;
                std::assert(builtin::std::core::member_count(readArray4.array) == 1, "Incorrect array length from section with offset 2");
                std::assert(sizeof(readArray4.array) == 1, "Incorrect array size in bytes from section with offset 2");
                std::assert(readArray4.array[0] == 0x36, "Incorrect array element value from section with offset 2");
            )";
        }
    };
}
