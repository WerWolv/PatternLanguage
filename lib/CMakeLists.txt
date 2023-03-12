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

        source/pl/core/token.cpp
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
        source/pl/lib/std/hash.cpp
)

target_compile_options(libpl PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-unknown-pragmas)

target_include_directories(libpl PUBLIC include)
target_link_libraries(libpl PRIVATE fmt::fmt-header-only)
target_link_libraries(libpl PUBLIC intervaltree libwolv-io libwolv-utils libwolv-hash)

set_property(TARGET libpl PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(libpl PROPERTIES PREFIX "")

if (LIBPL_SHARED_LIBRARY)
    install(TARGETS libpl DESTINATION lib)
endif ()