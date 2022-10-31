cmake_minimum_required(VERSION 3.16)
project(libpl-gen)

set(CMAKE_CXX_STANDARD 20)

add_library(libpl-gen INTERFACE)

target_include_directories(libpl-gen INTERFACE include)
target_link_libraries(libpl-gen INTERFACE libpl)
set_target_properties(libpl-gen PROPERTIES PREFIX "")

install(TARGETS libpl-gen DESTINATION lib)