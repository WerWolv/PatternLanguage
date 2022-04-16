cmake_minimum_required(VERSION 3.21)
project(PatternLanguage)

set(CMAKE_CXX_STANDARD 20)

option(LIBPL_SHARED_LIBRARY "Compile the library as a shared library" OFF)

add_subdirectory(external/fmt)
set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory(lib)
add_subdirectory(example)

add_library(pl::libpl ALIAS libpl)