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
        source/helpers/utils.cpp
        source/helpers/file.cpp

        source/pl/evaluator.cpp
        source/pl/lexer.cpp
        source/pl/log_console.cpp
        source/pl/parser.cpp
        source/pl/pattern_language.cpp
        source/pl/preprocessor.cpp
        source/pl/validator.cpp

        source/pl/libstd/std.cpp
        source/pl/libstd/mem.cpp
        source/pl/libstd/math.cpp
        source/pl/libstd/string.cpp
        source/pl/libstd/file.cpp
)

target_include_directories(libpl PUBLIC include)
target_link_libraries(libpl PUBLIC fmt::fmt)
set_property(TARGET libpl PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(libpl PROPERTIES PREFIX "")
