cmake_minimum_required(VERSION 3.16)
project(pattern_language_example)

set(CMAKE_CXX_STANDARD 20)

add_executable(pattern_language_example
    source/main.cpp
)

target_include_directories(pattern_language_example PRIVATE include)
target_link_libraries(pattern_language_example PRIVATE libpl)
