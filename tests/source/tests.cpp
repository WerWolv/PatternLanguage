#include <array>

#include "test_patterns/test_pattern_placement.hpp"
#include "test_patterns/test_pattern_structs.hpp"
#include "test_patterns/test_pattern_unions.hpp"
#include "test_patterns/test_pattern_enums.hpp"
#include "test_patterns/test_pattern_literals.hpp"
#include "test_patterns/test_pattern_padding.hpp"
#include "test_patterns/test_pattern_succeeding_assert.hpp"
#include "test_patterns/test_pattern_failing_assert.hpp"
#include "test_patterns/test_pattern_bitfields.hpp"
#include "test_patterns/test_pattern_math.hpp"
#include "test_patterns/test_pattern_match.hpp"
#include "test_patterns/test_pattern_rvalues.hpp"
#include "test_patterns/test_pattern_namespaces.hpp"
#include "test_patterns/test_pattern_extra_semicolon.hpp"
#include "test_patterns/test_pattern_pointers.hpp"
#include "test_patterns/test_pattern_arrays.hpp"
#include "test_patterns/test_pattern_nested_structs.hpp"
#include "test_patterns/test_pattern_attributes.hpp"
#include "test_patterns/test_pattern_struct_inheritance.hpp"
#include "test_patterns/test_pattern_doc_comments.hpp"
#include "test_patterns/test_pattern_strings.hpp"
#include "test_patterns/test_pattern_include.hpp"
#include "test_patterns/test_pattern_import.hpp"

std::array Tests = {
    TEST(Placement),
    TEST(Structs),
    TEST(Unions),
    TEST(Enums),
    TEST(Literals),
    TEST(Padding),
    TEST(DocComments),
    TEST(Strings),
    TEST(SucceedingAssert),
    TEST(FailingAssert),
    TEST(Bitfields),
    TEST(ReversedBitfields),
    TEST(Math),
    TEST(Matching),
    TEST(RValues),
    TEST(Namespaces),
    TEST(Include),
    TEST(Import),
    TEST(ExtraSemicolon),
    TEST(Pointers),
    TEST(Arrays),
    TEST(NestedStructs),
    TEST(Attributes),
    TEST(StructInheritance),
};