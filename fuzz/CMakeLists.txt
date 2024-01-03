cmake_minimum_required(VERSION 3.16)
project(plfuzz)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(plfuzz source/main.cpp)

target_link_libraries(plfuzz PRIVATE libpl libpl-gen fmt::fmt-header-only)
