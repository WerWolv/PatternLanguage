cmake_minimum_required(VERSION 3.16)
project(libpl)

set(CMAKE_CXX_STANDARD 20)

if (LIBPL_SHARED_LIBRARY)
    set(LIBRARY_TYPE SHARED)
    message(STATUS "libpl dynamic library is being created")
else ()
    set(LIBRARY_TYPE STATIC)
    message(STATUS "libpl static library is being created")
endif ()

add_library(libpl ${LIBRARY_TYPE}
        source/pl/helpers/utils.cpp
        source/pl/helpers/file.cpp

        source/pl/pattern_language.cpp
        source/pl/core/evaluator.cpp
        source/pl/core/lexer.cpp
        source/pl/core/parser.cpp
        source/pl/core/preprocessor.cpp
        source/pl/core/validator.cpp

        source/pl/lib/std/pragmas.cpp
        source/pl/lib/std/std.cpp
        source/pl/lib/std/mem.cpp
        source/pl/lib/std/math.cpp
        source/pl/lib/std/string.cpp
        source/pl/lib/std/file.cpp
        source/pl/lib/std/time.cpp
        source/pl/lib/std/core.cpp
)

target_precompile_headers(libpl PRIVATE
        include/pl.hpp
        include/pl/api.hpp
        include/pl/pattern_language.hpp
        include/pl/pattern_visitor.hpp

        include/pl/core/evaluator.hpp
        include/pl/core/lexer.hpp
        include/pl/core/log_console.hpp
        include/pl/core/parser.hpp
        include/pl/core/preprocessor.hpp
        include/pl/core/token.hpp
        include/pl/core/validator.hpp

        include/pl/core/errors/error.hpp
        include/pl/core/errors/evaluator_errors.hpp
        include/pl/core/errors/lexer_errors.hpp
        include/pl/core/errors/parser_errors.hpp
        include/pl/core/errors/preprocessor_errors.hpp
        include/pl/core/errors/validator_errors.hpp

        include/pl/core/ast/ast_node.hpp
        include/pl/core/ast/ast_node_array_variable_decl.hpp
        include/pl/core/ast/ast_node_attribute.hpp
        include/pl/core/ast/ast_node_bitfield.hpp
        include/pl/core/ast/ast_node_bitfield_field.hpp
        include/pl/core/ast/ast_node_builtin_type.hpp
        include/pl/core/ast/ast_node_cast.hpp
        include/pl/core/ast/ast_node_compound_statement.hpp
        include/pl/core/ast/ast_node_conditional_statement.hpp
        include/pl/core/ast/ast_node_control_flow_statement.hpp
        include/pl/core/ast/ast_node_enum.hpp
        include/pl/core/ast/ast_node_function_call.hpp
        include/pl/core/ast/ast_node_function_definition.hpp
        include/pl/core/ast/ast_node_literal.hpp
        include/pl/core/ast/ast_node_lvalue_assignment.hpp
        include/pl/core/ast/ast_node_mathematical_expression.hpp
        include/pl/core/ast/ast_node_multi_variable_decl.hpp
        include/pl/core/ast/ast_node_parameter_pack.hpp
        include/pl/core/ast/ast_node_pointer_variable_decl.hpp
        include/pl/core/ast/ast_node_rvalue.hpp
        include/pl/core/ast/ast_node_rvalue_assignment.hpp
        include/pl/core/ast/ast_node_scope_resolution.hpp
        include/pl/core/ast/ast_node_struct.hpp
        include/pl/core/ast/ast_node_ternary_expression.hpp
        include/pl/core/ast/ast_node_type_decl.hpp
        include/pl/core/ast/ast_node_type_operator.hpp
        include/pl/core/ast/ast_node_union.hpp
        include/pl/core/ast/ast_node_variable_decl.hpp
        include/pl/core/ast/ast_node_while_statement.hpp

        include/pl/helpers/concepts.hpp
        include/pl/helpers/file.hpp
        include/pl/helpers/fs.hpp
        include/pl/helpers/guards.hpp
        include/pl/helpers/types.hpp
        include/pl/helpers/utils.hpp

        include/pl/lib/std/libstd.hpp

        include/pl/patterns/pattern.hpp
        include/pl/patterns/pattern_array_dynamic.hpp
        include/pl/patterns/pattern_array_static.hpp
        include/pl/patterns/pattern_bitfield.hpp
        include/pl/patterns/pattern_boolean.hpp
        include/pl/patterns/pattern_character.hpp
        include/pl/patterns/pattern_enum.hpp
        include/pl/patterns/pattern_float.hpp
        include/pl/patterns/pattern_padding.hpp
        include/pl/patterns/pattern_pointer.hpp
        include/pl/patterns/pattern_signed.hpp
        include/pl/patterns/pattern_string.hpp
        include/pl/patterns/pattern_struct.hpp
        include/pl/patterns/pattern_union.hpp
        include/pl/patterns/pattern_unsigned.hpp
        include/pl/patterns/pattern_wide_character.hpp
        include/pl/patterns/pattern_wide_string.hpp
)

target_compile_options(libpl PRIVATE -Wall -Wextra -Werror -Wno-unknown-pragmas)
target_include_directories(libpl PUBLIC include)
target_link_libraries(libpl PUBLIC fmt::fmt intervaltree)
set_property(TARGET libpl PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(libpl PROPERTIES PREFIX "")
