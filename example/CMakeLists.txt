cmake_minimum_required(VERSION 3.21)
project(example)

set(CMAKE_CXX_STANDARD 20)

add_executable(example
    source/main.cpp
)

target_include_directories(example PRIVATE include)
target_link_libraries(example PRIVATE libpl)
