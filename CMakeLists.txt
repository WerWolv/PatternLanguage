cmake_minimum_required(VERSION 3.20)
project(PatternLanguage)

set(CMAKE_CXX_STANDARD 23)

option(LIBPL_SHARED_LIBRARY "Compile the library as a shared library" OFF)
option(LIBPL_ENABLE_TESTS "Enable testing" OFF)
option(LIBPL_ENABLE_CLI "Enable building the CLI tool" ON)
option(LIBPL_ENABLE_EXAMPLE "Enable building the examples" OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow")
endif()
if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-array-bounds")
endif()

if (WIN32)
    add_compile_definitions(OS_WINDOWS)
elseif (APPLE)
    add_compile_definitions(OS_MACOS)
elseif (UNIX AND NOT APPLE)
    add_compile_definitions(OS_LINUX)
endif ()

if (NOT TARGET fmt::fmt)
    add_subdirectory(external/fmt EXCLUDE_FROM_ALL)
    set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (NOT TARGET intervaltree)
    add_subdirectory(external/intervaltree)
    set_property(TARGET intervaltree PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

add_subdirectory(external/libwolv)

add_subdirectory(lib)
add_subdirectory(generators EXCLUDE_FROM_ALL)

if (LIBPL_ENABLE_EXAMPLE)
    add_subdirectory(example EXCLUDE_FROM_ALL)
endif ()

if (LIBPL_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
endif ()

if (LIBPL_ENABLE_CLI)
    add_subdirectory(cli)
endif ()

add_library(pl::libpl ALIAS libpl)
