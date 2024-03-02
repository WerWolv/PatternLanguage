cmake_minimum_required(VERSION 3.16)
project(libpl)

set(CMAKE_CXX_STANDARD 23)

if (LIBPL_SHARED_LIBRARY)
    set(LIBRARY_TYPE SHARED)
    message(STATUS "libpl dynamic library is being created")
else ()
    set(LIBRARY_TYPE STATIC)
    message(STATUS "libpl static library is being created")
endif ()

add_library(libpl ${LIBRARY_TYPE}
        source/pl/helpers/utils.cpp

        source/pl/pattern_language.cpp

        source/pl/core/ast/ast_node.cpp
        source/pl/core/ast/ast_node_array_variable_decl.cpp
        source/pl/core/ast/ast_node_attribute.cpp
        source/pl/core/ast/ast_node_bitfield.cpp
        source/pl/core/ast/ast_node_bitfield_array_variable_decl.cpp
        source/pl/core/ast/ast_node_bitfield_field.cpp
        source/pl/core/ast/ast_node_builtin_type.cpp
        source/pl/core/ast/ast_node_cast.cpp
        source/pl/core/ast/ast_node_compound_statement.cpp
        source/pl/core/ast/ast_node_conditional_statement.cpp
        source/pl/core/ast/ast_node_control_flow_statement.cpp
        source/pl/core/ast/ast_node_enum.cpp
        source/pl/core/ast/ast_node_function_call.cpp
        source/pl/core/ast/ast_node_function_definition.cpp
        source/pl/core/ast/ast_node_literal.cpp
        source/pl/core/ast/ast_node_lvalue_assignment.cpp
        source/pl/core/ast/ast_node_match_statement.cpp
        source/pl/core/ast/ast_node_mathematical_expression.cpp
        source/pl/core/ast/ast_node_multi_variable_decl.cpp
        source/pl/core/ast/ast_node_parameter_pack.cpp
        source/pl/core/ast/ast_node_pointer_variable_decl.cpp
        source/pl/core/ast/ast_node_rvalue.cpp
        source/pl/core/ast/ast_node_rvalue_assignment.cpp
        source/pl/core/ast/ast_node_scope_resolution.cpp
        source/pl/core/ast/ast_node_struct.cpp
        source/pl/core/ast/ast_node_ternary_expression.cpp
        source/pl/core/ast/ast_node_try_catch_statement.cpp
        source/pl/core/ast/ast_node_type_decl.cpp
        source/pl/core/ast/ast_node_type_operator.cpp
        source/pl/core/ast/ast_node_union.cpp
        source/pl/core/ast/ast_node_variable_decl.cpp
        source/pl/core/ast/ast_node_while_statement.cpp

        source/pl/core/token.cpp
        source/pl/core/evaluator.cpp
        source/pl/core/lexer.cpp
        source/pl/core/parser.cpp
        source/pl/core/preprocessor.cpp
        source/pl/core/validator.cpp
        source/pl/core/parser_manager.cpp

        source/pl/core/resolver.cpp
        source/pl/core/error.cpp

        source/pl/lib/std/pragmas.cpp
        source/pl/lib/std/std.cpp
        source/pl/lib/std/mem.cpp
        source/pl/lib/std/math.cpp
        source/pl/lib/std/string.cpp
        source/pl/lib/std/file.cpp
        source/pl/lib/std/time.cpp
        source/pl/lib/std/core.cpp
        source/pl/lib/std/hash.cpp
        source/pl/lib/std/random.cpp
        source/pl/core/resolvers.cpp
        source/pl/core/api.cpp
)

if (LIBPL_ENABLE_PRECOMPILED_HEADERS)
    file(GLOB_RECURSE TARGET_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp")
    set(SYSTEM_INCLUDES "<algorithm>;<array>;<atomic>;<chrono>;<cmath>;<cstddef>;<cstdint>;<cstdio>;<cstdlib>;<cstring>;<exception>;<filesystem>;<functional>;<iterator>;<limits>;<list>;<map>;<memory>;<optional>;<ranges>;<set>;<stdexcept>;<string>;<string_view>;<thread>;<tuple>;<type_traits>;<unordered_map>;<unordered_set>;<utility>;<variant>;<vector>")
    set(INCLUDES "${SYSTEM_INCLUDES};${TARGET_INCLUDES}")
    string(REPLACE ">" "$<ANGLE-R>" INCLUDES "${INCLUDES}")
    target_precompile_headers(libpl
            PUBLIC
            "$<$<COMPILE_LANGUAGE:CXX>:${INCLUDES}>"
    )
endif ()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libpl PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-unknown-pragmas -Wno-array-bounds)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(libpl PRIVATE -Wno-stringop-overflow)
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libpl PRIVATE -Wno-stringop-overread)
endif ()

target_include_directories(libpl PUBLIC include)
target_link_libraries(libpl PRIVATE ${FMT_LIBRARIES})
target_link_libraries(libpl PUBLIC wolv::io wolv::utils wolv::hash wolv::containers)

set_target_properties(libpl PROPERTIES PREFIX "")

if (LIBPL_SHARED_LIBRARY)
    install(TARGETS libpl DESTINATION lib)
endif ()
