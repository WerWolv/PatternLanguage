cmake_minimum_required(VERSION 3.21)
project(PatternLanguage)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/fmt)

add_subdirectory(lib)
add_subdirectory(example)

add_library(pl::libpl ALIAS libpl)