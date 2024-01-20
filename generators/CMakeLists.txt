cmake_minimum_required(VERSION 3.16)
project(libpl-gen)

set(CMAKE_CXX_STANDARD 23)

add_library(libpl-gen INTERFACE)

target_include_directories(libpl-gen INTERFACE include)
set_target_properties(libpl-gen PROPERTIES PREFIX "")