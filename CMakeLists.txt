cmake_minimum_required(VERSION 3.16)
project(PatternLanguage)

set(CMAKE_CXX_STANDARD 23)

option(LIBPL_SHARED_LIBRARY "Compile the library as a shared library" OFF)
option(LIBPL_ENABLE_TESTS "Enable testing" OFF)

if (NOT TARGET fmt::fmt)
    add_subdirectory(external/fmt EXCLUDE_FROM_ALL)
    set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (NOT TARGET intervaltree)
    add_subdirectory(external/intervaltree)
    set_property(TARGET intervaltree PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

add_subdirectory(lib)
add_subdirectory(example EXCLUDE_FROM_ALL)

if (LIBPL_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
endif ()

add_library(pl::libpl ALIAS libpl)
