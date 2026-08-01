#pragma once

#include "test_pattern.hpp"

namespace pl::test {

    class TestPatternExpressionSemantics : public TestPattern {
    public:
        TestPatternExpressionSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "ExpressionSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                // Arithmetic and precedence
                std::assert(2 + 3 * 4 == 14, "Multiplication precedence failed");
                std::assert((2 + 3) * 4 == 20, "Parenthesized addition failed");
                std::assert(24 / 3 / 2 == 4, "Division associativity failed");
                std::assert(24 / (3 / 2) == 24, "Parenthesized division failed");
                std::assert(20 - 6 - 4 == 10, "Subtraction associativity failed");
                std::assert(20 - (6 - 4) == 18, "Parenthesized subtraction failed");
                std::assert(17 % 5 == 2, "Positive modulo failed");
                std::assert(-17 % 5 == -2, "Negative modulo failed");
                std::assert(0 * 0xFFFFFFFF == 0, "Multiplication by zero failed");
                std::assert(7 * 6 + 5 == 47, "Mixed arithmetic failed");
                std::assert(7 + 6 * 5 == 37, "Mixed arithmetic precedence failed");
                std::assert(100 / 4 + 3 * 2 == 31, "Division/addition precedence failed");

                // Comparison boundaries
                std::assert(-2 < -1, "Negative less-than failed");
                std::assert(-1 > -2, "Negative greater-than failed");
                std::assert(-1 <= -1, "Negative less-than-or-equal failed");
                std::assert(-1 >= -1, "Negative greater-than-or-equal failed");
                std::assert(0 == 0U, "Mixed zero equality failed");
                std::assert(1 != -1, "Mixed signed inequality failed");
                std::assert(0.5F < 1.0D, "Float comparison failed");
                std::assert(1.5D >= 1.5F, "Mixed float comparison failed");

                // Boolean and ternary expressions
                std::assert(!(true && false), "Boolean and failed");
                std::assert(true || false, "Boolean or failed");
                std::assert(true ^^ false, "Boolean xor true case failed");
                std::assert(!(true ^^ true), "Boolean xor false case failed");
                std::assert((true ? 11 : 22) == 11, "True ternary branch failed");
                std::assert((false ? 11 : 22) == 22, "False ternary branch failed");
                std::assert((true ? (false ? 1 : 2) : 3) == 2, "Nested ternary failed");
                std::assert(((3 < 4) ? 8 : 9) == 8, "Comparison ternary failed");

                // Bitwise operations
                std::assert((0xF0 & 0xCC) == 0xC0, "Bitwise and failed");
                std::assert((0xF0 | 0x0F) == 0xFF, "Bitwise or failed");
                std::assert((0xAA ^ 0xFF) == 0x55, "Bitwise xor failed");
                std::assert((1 << 0) == 1, "Zero left shift failed");
                std::assert((1 << 7) == 128, "Left shift failed");
                std::assert((0x80 >> 7) == 1, "Right shift failed");
                std::assert(((0xF0 & 0xCC) | 0x03) == 0xC3, "Combined bitwise expression failed");
                std::assert(u8(~0x0F) == 0xF0, "Narrowed bitwise not failed");

                // Casts and narrowing
                std::assert(u8(0x123) == 0x23, "u8 narrowing failed");
                std::assert(u16(0x12345) == 0x2345, "u16 narrowing failed");
                std::assert(u32(-1) == 0xFFFFFFFF, "Signed-to-u32 cast failed");
                std::assert(s8(0x7F) == 127, "Positive s8 cast failed");
                std::assert(s8(0xFF) == -1, "Negative s8 cast failed");
                std::assert(s16(0xFFFF) == -1, "Negative s16 cast failed");
                std::assert(bool(0) == false, "Zero-to-bool cast failed");
                std::assert(bool(42) == true, "Integer-to-bool cast failed");
                std::assert(u32(3.9) == 3, "Float-to-integer cast failed");
                std::assert(double(3) == 3.0D, "Integer-to-double cast failed");

                // Compound assignments
                u32 compound = 10;
                compound += 5;
                std::assert(compound == 15, "Addition assignment failed");
                compound -= 3;
                std::assert(compound == 12, "Subtraction assignment failed");
                compound *= 4;
                std::assert(compound == 48, "Multiplication assignment failed");
                compound /= 6;
                std::assert(compound == 8, "Division assignment failed");
                compound %= 3;
                std::assert(compound == 2, "Modulo assignment failed");
                compound <<= 4;
                std::assert(compound == 32, "Left-shift assignment failed");
                compound >>= 2;
                std::assert(compound == 8, "Right-shift assignment failed");
                compound |= 3;
                std::assert(compound == 11, "Bitwise-or assignment failed");
                compound &= 6;
                std::assert(compound == 2, "Bitwise-and assignment failed");
                compound ^= 7;
                std::assert(compound == 5, "Bitwise-xor assignment failed");
            )";
        }
    };

    class TestPatternControlFlowSemantics : public TestPattern {
    public:
        TestPatternControlFlowSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "ControlFlowSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn classify(auto value) {
                    if (value < 0)
                        return -1;
                    else if (value == 0)
                        return 0;
                    else
                        return 1;
                };

                std::assert(classify(-9) == -1, "Negative conditional branch failed");
                std::assert(classify(0) == 0, "Zero conditional branch failed");
                std::assert(classify(9) == 1, "Positive conditional branch failed");

                fn sum_to(auto limit) {
                    u32 sum = 0;
                    u32 value = 1;
                    while (value <= limit) {
                        sum += value;
                        value += 1;
                    }
                    return sum;
                };

                std::assert(sum_to(0) == 0, "Zero-iteration while failed");
                std::assert(sum_to(1) == 1, "Single-iteration while failed");
                std::assert(sum_to(10) == 55, "Multi-iteration while failed");

                fn sum_odd_below(auto limit) {
                    u32 sum = 0;
                    for (u32 value = 0, value < limit, value += 1) {
                        if (value % 2 == 0)
                            continue;
                        sum += value;
                    }
                    return sum;
                };

                std::assert(sum_odd_below(1) == 0, "For continue lower bound failed");
                std::assert(sum_odd_below(6) == 9, "For continue failed");
                std::assert(sum_odd_below(10) == 25, "For accumulation failed");

                fn stop_at(auto limit) {
                    u32 value = 0;
                    while (true) {
                        if (value == limit)
                            break;
                        value += 1;
                    }
                    return value;
                };

                std::assert(stop_at(0) == 0, "Immediate break failed");
                std::assert(stop_at(7) == 7, "Delayed break failed");

                fn nested_loops() {
                    u32 count = 0;
                    for (u32 outer = 0, outer < 3, outer += 1) {
                        for (u32 inner = 0, inner < 4, inner += 1) {
                            if (inner == 2)
                                break;
                            count += 1;
                        }
                    }
                    return count;
                };

                std::assert(nested_loops() == 6, "Nested loop break failed");

                fn match_value(auto value) {
                    match (value) {
                        (0): return 100;
                        (1 | 2): return 200;
                        (3 ... 5): return 300;
                        (_): return 400;
                    }
                };

                std::assert(match_value(0) == 100, "Exact match failed");
                std::assert(match_value(1) == 200, "First alternative match failed");
                std::assert(match_value(2) == 200, "Second alternative match failed");
                std::assert(match_value(3) == 300, "Range match lower bound failed");
                std::assert(match_value(5) == 300, "Range match upper bound failed");
                std::assert(match_value(6) == 400, "Wildcard match failed");

                fn match_pair(auto first, auto second) {
                    match (first, second) {
                        (1, 2): return 12;
                        (1, 3 ... 9): return 10;
                        (3 ... 9, 2): return 2;
                        (_, _): return 0;
                    }
                };

                std::assert(match_pair(1, 2) == 12, "Two-parameter exact match failed");
                std::assert(match_pair(1, 9) == 10, "Two-parameter first wildcard failed");
                std::assert(match_pair(9, 2) == 2, "Two-parameter second wildcard failed");
                std::assert(match_pair(9, 9) == 0, "Two-parameter default failed");
            )";
        }
    };

    class TestPatternFunctionSemantics : public TestPattern {
    public:
        TestPatternFunctionSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "FunctionSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn add(auto left, auto right) {
                    return left + right;
                };

                fn add_defaults(auto left, auto right = 7) {
                    return left + right;
                };

                std::assert(add(2, 3) == 5, "Two-argument function failed");
                std::assert(add(-2, 3) == 1, "Signed function arguments failed");
                std::assert(add_defaults(5) == 12, "Default argument failed");
                std::assert(add_defaults(5, 9) == 14, "Default argument override failed");

                fn factorial(auto value) {
                    if (value <= 1)
                        return 1;
                    return value * factorial(value - 1);
                };

                std::assert(factorial(0) == 1, "Factorial zero case failed");
                std::assert(factorial(1) == 1, "Factorial one case failed");
                std::assert(factorial(5) == 120, "Recursive factorial failed");

                fn fibonacci(auto value) {
                    if (value < 2)
                        return value;
                    return fibonacci(value - 1) + fibonacci(value - 2);
                };

                std::assert(fibonacci(0) == 0, "Fibonacci zero case failed");
                std::assert(fibonacci(1) == 1, "Fibonacci one case failed");
                std::assert(fibonacci(2) == 1, "Fibonacci two case failed");
                std::assert(fibonacci(6) == 8, "Recursive fibonacci failed");

                fn mutate_array(ref u32 values, auto index, auto value) {
                    values[index] = value;
                };

                u32 mutableArray[3] = { 1, 2, 3 };
                mutate_array(mutableArray, 0, 9);
                mutate_array(mutableArray, 2, 7);
                std::assert(mutableArray[0] == 9, "Reference array first mutation failed");
                std::assert(mutableArray[1] == 2, "Reference array untouched element changed");
                std::assert(mutableArray[2] == 7, "Reference array last mutation failed");

                fn select(auto condition, auto left, auto right) {
                    if (condition)
                        return left;
                    return right;
                };

                std::assert(select(true, 11, 22) == 11, "Function early true return failed");
                std::assert(select(false, 11, 22) == 22, "Function fallthrough return failed");

                fn nested_scope(auto value) {
                    if (true) {
                        u32 nestedValue = 99;
                        std::assert(nestedValue == 99, "Nested function local failed");
                    }
                    return value;
                };

                std::assert(nested_scope(12) == 12, "Function parameter changed by nested scope");
            )";
        }
    };

    class TestPatternLocalStorageSemantics : public TestPattern {
    public:
        TestPatternLocalStorageSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "LocalStorageSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                struct Pair {
                    u32 first;
                    u32 second;
                };

                struct Container {
                    Pair pair;
                    u32 values[3];
                };

                Pair pair;
                pair.first = 10;
                pair.second = 20;
                std::assert(pair.first == 10, "Local struct first member failed");
                std::assert(pair.second == 20, "Local struct second member failed");

                Pair copiedPair = pair;
                std::assert(copiedPair.first == 10, "Local struct copy first member failed");
                std::assert(copiedPair.second == 20, "Local struct copy second member failed");
                copiedPair.first = 99;
                std::assert(pair.first == 10, "Local struct copy aliased source");
                std::assert(copiedPair.first == 99, "Local struct copy mutation failed");

                Pair pairs[3];
                pairs[0].first = 1;
                pairs[0].second = 2;
                pairs[1].first = 3;
                pairs[1].second = 4;
                pairs[2].first = 5;
                pairs[2].second = 6;
                std::assert(pairs[0].first == 1 && pairs[0].second == 2, "Local struct array first entry failed");
                std::assert(pairs[1].first == 3 && pairs[1].second == 4, "Local struct array middle entry failed");
                std::assert(pairs[2].first == 5 && pairs[2].second == 6, "Local struct array last entry failed");

                pairs[2] = pairs[0];
                std::assert(pairs[0].first == 1 && pairs[0].second == 2, "Struct array copy changed source");
                std::assert(pairs[2].first == 1 && pairs[2].second == 2, "Struct array copy failed");

                u32 values[5] = { 2, 4, 6, 8, 10 };
                std::assert(values[0] == 2, "Array initializer first element failed");
                std::assert(values[1] == 4, "Array initializer second element failed");
                std::assert(values[2] == 6, "Array initializer third element failed");
                std::assert(values[3] == 8, "Array initializer fourth element failed");
                std::assert(values[4] == 10, "Array initializer fifth element failed");

                values[1] = values[4];
                values[3] = values[0] + values[2];
                std::assert(values[1] == 10, "Array indexed assignment failed");
                std::assert(values[3] == 8, "Array expression assignment failed");

                Container container;
                container.pair.first = 11;
                container.pair.second = 12;
                container.values[0] = 13;
                container.values[1] = 14;
                container.values[2] = 15;
                std::assert(container.pair.first == 11, "Nested struct first member failed");
                std::assert(container.pair.second == 12, "Nested struct second member failed");
                std::assert(container.values[0] == 13, "Nested array first member failed");
                std::assert(container.values[1] == 14, "Nested array middle member failed");
                std::assert(container.values[2] == 15, "Nested array last member failed");

                Container copiedContainer = container;
                copiedContainer.pair.first = 100;
                copiedContainer.values[1] = 200;
                std::assert(container.pair.first == 11, "Nested struct copy aliased source member");
                std::assert(container.values[1] == 14, "Nested array copy aliased source element");
                std::assert(copiedContainer.pair.first == 100, "Nested struct copy mutation failed");
                std::assert(copiedContainer.values[1] == 200, "Nested array copy mutation failed");

                fn make_pair(auto first, auto second) {
                    Pair result;
                    result.first = first;
                    result.second = second;
                    return result;
                };

                Pair returnedPair = make_pair(21, 22);
                std::assert(returnedPair.first == 21, "Returned struct first member failed");
                std::assert(returnedPair.second == 22, "Returned struct second member failed");

                str names[3];
                names[0] = "zero";
                names[1] = "one";
                names[2] = "two";
                std::assert(names[0] == "zero", "String array first element failed");
                std::assert(names[1] == "one", "String array middle element failed");
                std::assert(names[2] == "two", "String array last element failed");
                names[1] = names[0];
                std::assert(names[0] == "zero", "String array copy changed source");
                std::assert(names[1] == "zero", "String array copy failed");
                std::assert(names[2] == "two", "String array copy changed neighbor");
            )";
        }
    };

    class TestPatternStringSemantics : public TestPattern {
    public:
        TestPatternStringSemantics(core::Evaluator *evaluator) : TestPattern(evaluator, "StringSemantics") { }

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                std::assert("" == "", "Empty string equality failed");
                std::assert("abc" == "abc", "String equality failed");
                std::assert("abc" != "abd", "String inequality failed");
                std::assert("abc" < "abd", "String less-than failed");
                std::assert("abd" > "abc", "String greater-than failed");
                std::assert("abc" <= "abc", "String less-than-or-equal failed");
                std::assert("abc" >= "abc", "String greater-than-or-equal failed");
                std::assert("hello" + " world" == "hello world", "String concatenation failed");
                std::assert("A" + 'B' == "AB", "String and character concatenation failed");
                std::assert('A' + "BC" == "ABC", "Character and string concatenation failed");
                std::assert("ab" * 0 == "", "Zero string repetition failed");
                std::assert("ab" * 1 == "ab", "Single string repetition failed");
                std::assert("ab" * 3 == "ababab", "String repetition failed");

                str escaped = "line1\nline2\tend";
                std::assert(escaped == "line1\nline2\tend", "Escaped string storage failed");
                std::assert("\"quoted\"" == "\x22quoted\x22", "Hex escape equality failed");
                std::assert("\\" == "\x5C", "Backslash escape failed");

                str left = "left";
                str right = "right";
                str combined = left + ":" + right;
                std::assert(left == "left", "String concatenation changed left operand");
                std::assert(right == "right", "String concatenation changed right operand");
                std::assert(combined == "left:right", "String variable concatenation failed");

                str copied = combined;
                combined = "changed";
                std::assert(copied == "left:right", "String copy aliased source");
                std::assert(combined == "changed", "String reassignment failed");

                fn decorate(auto value) {
                    return "[" + value + "]";
                };

                std::assert(decorate("") == "[]", "Empty string function argument failed");
                std::assert(decorate("value") == "[value]", "String function return failed");

                std::assert(builtin::std::format("{}", 42) == "42", "Integer formatting failed");
                std::assert(builtin::std::format("{}", -42) == "-42", "Signed integer formatting failed");
                std::assert(builtin::std::format("{}", "text") == "text", "String formatting failed");
                std::assert(builtin::std::format("{}:{}", "key", 7) == "key:7", "Multi-argument formatting failed");
                std::assert(builtin::std::format("{{{}}}", 9) == "{9}", "Escaped brace formatting failed");
            )";
        }
    };

}
