cmake_minimum_required(VERSION 3.16)
project(plformatters)

set(CMAKE_CXX_STANDARD 20)

add_library(plformatters INTERFACE)

target_include_directories(plformatters INTERFACE include)
target_link_libraries(plformatters INTERFACE libpl)