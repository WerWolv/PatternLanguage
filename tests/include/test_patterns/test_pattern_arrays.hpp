#pragma once

#include "test_pattern.hpp"

#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>

#include <vector>

namespace pl::test {

    class TestPatternArrays : public TestPattern {
    public:
        TestPatternArrays(core::Evaluator *evaluator) : TestPattern(evaluator, "Arrays") {
            auto first = create<PatternArrayStatic>("u8", "first", 0x0, sizeof(u8[4]), 0);
            first->setEntries(create<PatternUnsigned>("u8", "", 0x0, sizeof(u8), 0), 4);

            auto second = create<PatternArrayStatic>("u8", "second", 0x4, sizeof(u8[4]), 0);
            second->setEntries(create<PatternUnsigned>("u8", "", 0x4, sizeof(u8), 0), 4);

            auto testStruct = create<PatternStruct>("Signature", "sign", 0x0, sizeof(u8[8]), 0);
            std::vector<std::shared_ptr<Pattern>> structMembers;
            {
                structMembers.push_back(std::move(first));
                structMembers.push_back(std::move(second));
            }
            testStruct->setEntries(std::move(structMembers));

            addPattern(std::move(testStruct));
        }
        ~TestPatternArrays() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn end_of_signature() {
                    return $ >= 8;
                };

                struct Signature {
                    u8 first[4];
                    u8 second[while(!end_of_signature())];
                };

                Signature sign @ 0x0;

                std::assert(sign.first[0] == 0x89, "Invalid 1st byte of signature");
                std::assert(sign.first[1] == 0x50, "Invalid 2nd byte of signature");
                std::assert(sign.first[2] == 0x4E, "Invalid 3rd byte of signature");
                std::assert(sign.first[3] == 0x47, "Invalid 4th byte of signature");
                std::assert(sizeof(sign.second) == 4, "Invalid size of signature");
                std::assert(sign.second[0] == 0x0D, "Invalid 5th byte of signature");
                std::assert(sign.second[1] == 0x0A, "Invalid 6th byte of signature");
                std::assert(sign.second[2] == 0x1A, "Invalid 7th byte of signature");
                std::assert(sign.second[3] == 0x0A, "Invalid 8th byte of signature");

                fn forward(auto fmt, auto ... args) {
                    builtin::std::format(fmt, args);
                };

                fn test_local_array_forwarding(auto size) {
                    u8 count = 6;
                    char buffer[count];

                    if (size < count)
                        count = 0;

                    forward(buffer);

                    return count;
                };

                test_local_array_forwarding(65);

                struct LocalPair {
                    u8 a;
                    u8 b;
                };

                fn test_local_struct_layout() {
                    LocalPair pair;
                    pair.a = 1;
                    pair.b = 2;

                    std::assert(pair.a == 1, "Local struct field a invalid");
                    std::assert(pair.b == 2, "Local struct field b invalid");
                };

                fn test_local_struct_array_assignment() {
                    LocalPair pairs[2];

                    pairs[0].a = 1;
                    pairs[0].b = 2;
                    pairs[1].a = 9;
                    pairs[1].b = 9;

                    pairs[1] = pairs[0];

                    std::assert(pairs[0].a == 1, "Local array source field a changed");
                    std::assert(pairs[0].b == 2, "Local array source field b changed");
                    std::assert(pairs[1].a == 1, "Local array destination field a invalid");
                    std::assert(pairs[1].b == 2, "Local array destination field b invalid");
                };

                fn make_local_pair(auto a, auto b) {
                    LocalPair pair;
                    pair.a = a;
                    pair.b = b;
                    return pair;
                };

                fn forward_local_pair(auto pair) {
                    return pair;
                };

                fn test_local_struct_auto_return() {
                    auto pair = make_local_pair(3, 4);

                    std::assert(pair.a == 3, "Returned local struct field a invalid");
                    std::assert(pair.b == 4, "Returned local struct field b invalid");
                };

                fn test_local_struct_auto_forwarding() {
                    LocalPair pair;
                    pair.a = 5;
                    pair.b = 6;

                    auto forwarded = forward_local_pair(pair);

                    std::assert(forwarded.a == 5, "Forwarded local struct field a invalid");
                    std::assert(forwarded.b == 6, "Forwarded local struct field b invalid");
                };

                fn test_local_struct_return_array_assignment() {
                    LocalPair pairs[2];
                    pairs[0].a = 7;
                    pairs[0].b = 8;
                    pairs[1] = make_local_pair(9, 10);

                    std::assert(pairs[0].a == 7, "Returned local array source field a changed");
                    std::assert(pairs[0].b == 8, "Returned local array source field b changed");
                    std::assert(pairs[1].a == 9, "Returned local array destination field a invalid");
                    std::assert(pairs[1].b == 10, "Returned local array destination field b invalid");
                };

                test_local_struct_layout();
                test_local_struct_array_assignment();
                test_local_struct_auto_return();
                test_local_struct_auto_forwarding();
                test_local_struct_return_array_assignment();
            )";
        }
    };

}
