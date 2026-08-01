#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternTypeOperatorSemantics : public TestPattern {
    public:
        TestPatternTypeOperatorSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "TypeOperatorSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                std::assert(sizeof(u8) == 1, "sizeof(u8) failed");
                std::assert(sizeof(s8) == 1, "sizeof(s8) failed");
                std::assert(sizeof(u16) == 2, "sizeof(u16) failed");
                std::assert(sizeof(s16) == 2, "sizeof(s16) failed");
                std::assert(sizeof(u24) == 3, "sizeof(u24) failed");
                std::assert(sizeof(s24) == 3, "sizeof(s24) failed");
                std::assert(sizeof(u32) == 4, "sizeof(u32) failed");
                std::assert(sizeof(s32) == 4, "sizeof(s32) failed");
                std::assert(sizeof(u48) == 6, "sizeof(u48) failed");
                std::assert(sizeof(s48) == 6, "sizeof(s48) failed");
                std::assert(sizeof(u64) == 8, "sizeof(u64) failed");
                std::assert(sizeof(s64) == 8, "sizeof(s64) failed");
                std::assert(sizeof(u96) == 12, "sizeof(u96) failed");
                std::assert(sizeof(s96) == 12, "sizeof(s96) failed");
                std::assert(sizeof(u128) == 16, "sizeof(u128) failed");
                std::assert(sizeof(s128) == 16, "sizeof(s128) failed");
                std::assert(sizeof(char) == 1, "sizeof(char) failed");
                std::assert(sizeof(char16) == 2, "sizeof(char16) failed");
                std::assert(sizeof(bool) == 1, "sizeof(bool) failed");
                std::assert(sizeof(float) == 4, "sizeof(float) failed");
                std::assert(sizeof(double) == 8, "sizeof(double) failed");

                std::assert(typenameof(u8) == "u8", "typenameof(u8) failed");
                std::assert(typenameof(s8) == "s8", "typenameof(s8) failed");
                std::assert(typenameof(u16) == "u16", "typenameof(u16) failed");
                std::assert(typenameof(s16) == "s16", "typenameof(s16) failed");
                std::assert(typenameof(u24) == "u24", "typenameof(u24) failed");
                std::assert(typenameof(s24) == "s24", "typenameof(s24) failed");
                std::assert(typenameof(u32) == "u32", "typenameof(u32) failed");
                std::assert(typenameof(s32) == "s32", "typenameof(s32) failed");
                std::assert(typenameof(u48) == "u48", "typenameof(u48) failed");
                std::assert(typenameof(s48) == "s48", "typenameof(s48) failed");
                std::assert(typenameof(u64) == "u64", "typenameof(u64) failed");
                std::assert(typenameof(s64) == "s64", "typenameof(s64) failed");
                std::assert(typenameof(u96) == "u96", "typenameof(u96) failed");
                std::assert(typenameof(s96) == "s96", "typenameof(s96) failed");
                std::assert(typenameof(u128) == "u128", "typenameof(u128) failed");
                std::assert(typenameof(s128) == "s128", "typenameof(s128) failed");
                std::assert(typenameof(char) == "char", "typenameof(char) failed");
                std::assert(typenameof(char16) == "char16", "typenameof(char16) failed");
                std::assert(typenameof(bool) == "bool", "typenameof(bool) failed");
                std::assert(typenameof(float) == "float", "typenameof(float) failed");
                std::assert(typenameof(double) == "double", "typenameof(double) failed");

                struct TypeOperatorStruct {
                    u8 first;
                    u16 second;
                    u32 third;
                };

                union TypeOperatorUnion {
                    u8 small;
                    u64 large;
                };

                enum TypeOperatorEnum : u16 { A, B };

                std::assert(sizeof(TypeOperatorStruct) == 7, "Custom struct type size failed");
                std::assert(sizeof(TypeOperatorUnion) == 8, "Custom union type size failed");
                std::assert(sizeof(TypeOperatorEnum) == 2, "Custom enum type size failed");
                std::assert(typenameof(TypeOperatorStruct) == "TypeOperatorStruct", "Custom struct type name failed");
                std::assert(typenameof(TypeOperatorUnion) == "TypeOperatorUnion", "Custom union type name failed");
                std::assert(typenameof(TypeOperatorEnum) == "TypeOperatorEnum", "Custom enum type name failed");

                TypeOperatorStruct typeOperatorValue;
                std::assert(sizeof(typeOperatorValue) == 7, "Struct value size failed");
                std::assert(typenameof(typeOperatorValue) == "TypeOperatorStruct", "Struct value type name failed");
            )";
        }
    };

    class TestPatternEnumSemantics : public TestPattern {
    public:
        TestPatternEnumSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "EnumSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                enum Direction : u8 {
                    North,
                    East,
                    South = 5,
                    West,
                };

                std::assert(Direction::North == 0, "Implicit first enum value failed");
                std::assert(Direction::East == 1, "Implicit second enum value failed");
                std::assert(Direction::South == 5, "Explicit enum value failed");
                std::assert(Direction::West == 6, "Implicit value after explicit value failed");
                std::assert(Direction::North != Direction::East, "Enum inequality failed");
                std::assert(Direction::West > Direction::South, "Enum ordering failed");

                Direction direction = Direction::North;
                std::assert(direction == Direction::North, "Local enum initialization failed");
                direction = Direction::West;
                std::assert(direction == Direction::West, "Local enum assignment failed");
                std::assert(u8(direction) == 6, "Enum-to-integer cast failed");
                std::assert(builtin::std::format("{}", direction) == "Direction::West", "Enum formatting failed");

                enum SignedCode : s8 {
                    Minimum = -128,
                    Negative = -2,
                    MinusOne,
                    Zero,
                    Positive = 127,
                };

                std::assert(SignedCode::Minimum == -128, "Minimum signed enum value failed");
                std::assert(SignedCode::Negative == -2, "Negative signed enum value failed");
                std::assert(SignedCode::MinusOne == -1, "Incremented negative enum value failed");
                std::assert(SignedCode::Zero == 0, "Incremented zero enum value failed");
                std::assert(SignedCode::Positive == 127, "Maximum signed enum value failed");

                SignedCode signedCode = SignedCode::MinusOne;
                std::assert(builtin::std::format("{}", signedCode) == "SignedCode::MinusOne", "Negative enum formatting failed");

                enum RangeCode : u8 {
                    Low = 0 ... 9,
                    Medium = 10 ... 19,
                    High = 20 ... 29,
                };

                RangeCode rangeCode = 0;
                std::assert(builtin::std::format("{}", rangeCode) == "RangeCode::Low", "Range enum lower bound failed");
                rangeCode = 9;
                std::assert(builtin::std::format("{}", rangeCode) == "RangeCode::Low", "Range enum upper bound failed");
                rangeCode = 10;
                std::assert(builtin::std::format("{}", rangeCode) == "RangeCode::Medium", "Second range lower bound failed");
                rangeCode = 19;
                std::assert(builtin::std::format("{}", rangeCode) == "RangeCode::Medium", "Second range upper bound failed");
                rangeCode = 25;
                std::assert(builtin::std::format("{}", rangeCode) == "RangeCode::High", "Range enum middle value failed");
                std::assert(builtin::std::format("{}", rangeCode) == "RangeCode::High", "Range enum formatting failed");

                enum Flags : u8 {
                    Read = 1,
                    Write = 2,
                    Execute = 4,
                };

                std::assert((u8(Flags::Read) | u8(Flags::Write)) == 3, "Enum bitwise combination failed");
                std::assert((u8(Flags::Execute) & 4) == 4, "Enum bitwise mask failed");
            )";
        }
    };

    class TestPatternNamespaceSemantics : public TestPattern {
    public:
        TestPatternNamespaceSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "NamespaceSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                namespace First {
                    fn value() { return 11; };
                    fn add(auto left, auto right) { return left + right; };

                    struct Item {
                        u32 value;
                    };

                    enum Kind : u8 { One = 1, Two = 2 };

                    namespace Nested {
                        fn value() { return 22; };

                        struct Item {
                            u16 value;
                        };
                    }
                }

                namespace Second {
                    fn value() { return 33; };

                    struct Item {
                        u64 value;
                    };

                    enum Kind : u8 { One = 10, Two = 20 };
                }

                std::assert(First::value() == 11, "First namespace function failed");
                std::assert(First::Nested::value() == 22, "Nested namespace function failed");
                std::assert(Second::value() == 33, "Second namespace function failed");
                std::assert(First::add(4, 5) == 9, "Namespaced function arguments failed");

                First::Item firstItem;
                First::Nested::Item nestedItem;
                Second::Item secondItem;
                firstItem.value = 101;
                nestedItem.value = 202;
                secondItem.value = 303;

                std::assert(firstItem.value == 101, "First namespaced type failed");
                std::assert(nestedItem.value == 202, "Nested namespaced type failed");
                std::assert(secondItem.value == 303, "Second namespaced type failed");
                std::assert(sizeof(firstItem) == 4, "First namespaced type size failed");
                std::assert(sizeof(nestedItem) == 2, "Nested namespaced type size failed");
                std::assert(sizeof(secondItem) == 8, "Second namespaced type size failed");
                std::assert(typenameof(firstItem) == "First::Item", "First namespaced type name failed");
                std::assert(typenameof(nestedItem) == "First::Nested::Item", "Nested namespaced type name failed");
                std::assert(typenameof(secondItem) == "Second::Item", "Second namespaced type name failed");

                std::assert(First::Kind::One == 1, "First namespaced enum failed");
                std::assert(First::Kind::Two == 2, "First namespaced enum second value failed");
                std::assert(Second::Kind::One == 10, "Second namespaced enum failed");
                std::assert(Second::Kind::Two == 20, "Second namespaced enum second value failed");

                First::Kind firstKind = First::Kind::Two;
                Second::Kind secondKind = Second::Kind::Two;
                std::assert(firstKind == 2, "Namespaced enum local initialization failed");
                std::assert(secondKind == 20, "Second namespaced enum local initialization failed");
            )";
        }
    };

    class TestPatternArrayAlgorithmSemantics : public TestPattern {
    public:
        TestPatternArrayAlgorithmSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "ArrayAlgorithmSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn array_sum(ref u32 values, auto count) {
                    u32 result = 0;
                    for (u32 index = 0, index < count, index += 1)
                        result += values[index];
                    return result;
                };

                fn array_min(ref u32 values, auto count) {
                    u32 result = values[0];
                    for (u32 index = 1, index < count, index += 1) {
                        if (values[index] < result)
                            result = values[index];
                    }
                    return result;
                };

                fn array_max(ref u32 values, auto count) {
                    u32 result = values[0];
                    for (u32 index = 1, index < count, index += 1) {
                        if (values[index] > result)
                            result = values[index];
                    }
                    return result;
                };

                fn array_reverse(ref u32 values, auto count) {
                    for (u32 index = 0, index < count / 2, index += 1) {
                        u32 temporary = values[index];
                        values[index] = values[count - index - 1];
                        values[count - index - 1] = temporary;
                    }
                };

                fn array_find(ref u32 values, auto count, auto needle) {
                    for (u32 index = 0, index < count, index += 1) {
                        if (values[index] == needle)
                            return index;
                    }
                    return -1;
                };

                u32 values[6] = { 9, 2, 7, 4, 1, 8 };
                std::assert(array_sum(values, 6) == 31, "Array sum failed");
                std::assert(array_sum(values, 1) == 9, "Array partial sum failed");
                std::assert(array_sum(values, 0) == 0, "Empty array sum failed");
                std::assert(array_min(values, 6) == 1, "Array minimum failed");
                std::assert(array_max(values, 6) == 9, "Array maximum failed");
                std::assert(array_find(values, 6, 9) == 0, "Array find first failed");
                std::assert(array_find(values, 6, 4) == 3, "Array find middle failed");
                std::assert(array_find(values, 6, 8) == 5, "Array find last failed");
                std::assert(array_find(values, 6, 100) == -1, "Array find missing failed");

                array_reverse(values, 6);
                std::assert(values[0] == 8, "Even array reverse index 0 failed");
                std::assert(values[1] == 1, "Even array reverse index 1 failed");
                std::assert(values[2] == 4, "Even array reverse index 2 failed");
                std::assert(values[3] == 7, "Even array reverse index 3 failed");
                std::assert(values[4] == 2, "Even array reverse index 4 failed");
                std::assert(values[5] == 9, "Even array reverse index 5 failed");
                std::assert(array_sum(values, 6) == 31, "Reverse changed array sum");

                u32 oddValues[5] = { 1, 2, 3, 4, 5 };
                array_reverse(oddValues, 5);
                std::assert(oddValues[0] == 5, "Odd array reverse index 0 failed");
                std::assert(oddValues[1] == 4, "Odd array reverse index 1 failed");
                std::assert(oddValues[2] == 3, "Odd array reverse center failed");
                std::assert(oddValues[3] == 2, "Odd array reverse index 3 failed");
                std::assert(oddValues[4] == 1, "Odd array reverse index 4 failed");

                u32 dynamicCount = 4;
                u32 dynamicValues[dynamicCount];
                for (u32 index = 0, index < dynamicCount, index += 1)
                    dynamicValues[index] = index * index;
                std::assert(dynamicValues[0] == 0, "Dynamic array first value failed");
                std::assert(dynamicValues[1] == 1, "Dynamic array second value failed");
                std::assert(dynamicValues[2] == 4, "Dynamic array third value failed");
                std::assert(dynamicValues[3] == 9, "Dynamic array fourth value failed");
                std::assert(array_sum(dynamicValues, dynamicCount) == 14, "Dynamic array function argument failed");

                u32 copiedValues[6];
                for (u32 copyIndex = 0, copyIndex < 6, copyIndex += 1)
                    copiedValues[copyIndex] = values[copyIndex];
                std::assert(copiedValues[0] == 8, "Array copy first value failed");
                std::assert(copiedValues[5] == 9, "Array copy last value failed");
                copiedValues[0] = 100;
                std::assert(values[0] == 8, "Array copy aliased source");
                std::assert(copiedValues[0] == 100, "Array copy mutation failed");
            )";
        }
    };

    class TestPatternAliasAggregateSemantics : public TestPattern {
    public:
        TestPatternAliasAggregateSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "AliasAggregateSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                using Byte = u8;
                using Word = u16;
                using Identity<T> = T;

                Byte byteValue = 0xAB;
                Word wordValue = 0x1234;
                Identity<u32> identityValue = 0x12345678;
                std::assert(byteValue == 0xAB, "Primitive byte alias failed");
                std::assert(wordValue == 0x1234, "Primitive word alias failed");
                std::assert(identityValue == 0x12345678, "Generic identity alias failed");
                std::assert(sizeof(Byte) == 1, "Primitive alias size failed");
                std::assert(sizeof(Identity<u64>) == 8, "Generic alias size failed");

                struct Box<T> {
                    T value;
                };

                struct Pair<T, U> {
                    T first;
                    U second;
                };

                using U32Box = Box<u32>;
                using BoxOf<T> = Box<T>;
                using NestedBox<T> = Box<Box<T>>;

                U32Box u32Box;
                u32Box.value = 42;
                std::assert(u32Box.value == 42, "Concrete struct alias failed");
                std::assert(sizeof(u32Box) == 4, "Concrete struct alias size failed");

                BoxOf<u16> genericBox;
                genericBox.value = 77;
                std::assert(genericBox.value == 77, "Generic struct alias failed");
                std::assert(sizeof(genericBox) == 2, "Generic struct alias size failed");

                NestedBox<u8> nestedBox;
                nestedBox.value.value = 88;
                std::assert(nestedBox.value.value == 88, "Nested generic alias failed");
                std::assert(sizeof(nestedBox) == 1, "Nested generic alias size failed");

                Pair<u8, u32> mixedPair;
                mixedPair.first = 1;
                mixedPair.second = 2;
                std::assert(mixedPair.first == 1, "Mixed generic first member failed");
                std::assert(mixedPair.second == 2, "Mixed generic second member failed");
                std::assert(sizeof(mixedPair) == 5, "Mixed generic struct size failed");

                struct SizedArray<T, auto Count> {
                    T values[Count];
                };

                SizedArray<u16, 3> sizedArray;
                sizedArray.values[0] = 10;
                sizedArray.values[1] = 20;
                sizedArray.values[2] = 30;
                std::assert(sizedArray.values[0] == 10, "Value-template first element failed");
                std::assert(sizedArray.values[1] == 20, "Value-template second element failed");
                std::assert(sizedArray.values[2] == 30, "Value-template third element failed");
                std::assert(sizeof(sizedArray) == 6, "Value-template size failed");

                struct Base<T> {
                    T base;
                };

                struct Derived<T> : Base<T> {
                    T derived;
                };

                Derived<u32> derivedValue;
                derivedValue.base = 100;
                derivedValue.derived = 200;
                std::assert(derivedValue.base == 100, "Local inherited member failed");
                std::assert(derivedValue.derived == 200, "Local derived member failed");
                std::assert(sizeof(derivedValue) == 8, "Local derived type size failed");

                Derived<u32> copiedDerived = derivedValue;
                copiedDerived.base = 300;
                copiedDerived.derived = 400;
                std::assert(derivedValue.base == 100, "Derived copy aliased base member");
                std::assert(derivedValue.derived == 200, "Derived copy aliased own member");
                std::assert(copiedDerived.base == 300, "Derived copy base mutation failed");
                std::assert(copiedDerived.derived == 400, "Derived copy own mutation failed");

                union Overlay {
                    u32 whole;
                    u8 bytes[4];
                };

                Overlay overlay;
                overlay.whole = 0;
                overlay.bytes[0] = 1;
                std::assert(overlay.whole != 0, "Local union members do not overlap");
                overlay.whole = 0;
                std::assert(overlay.bytes[0] == 0, "Local union write did not update array member");
                std::assert(sizeof(overlay) == 4, "Local union size failed");
            )";
        }
    };

}
