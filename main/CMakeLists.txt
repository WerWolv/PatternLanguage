cmake_minimum_required(VERSION 3.16)
project(plcli)

set(CMAKE_CXX_STANDARD 20)

add_executable(plcli
        source/main.cpp
)

add_subdirectory(../external/cli11 ../external/cli11)

target_compile_options(plcli PRIVATE -Wall -Wextra -Werror -Wno-unknown-pragmas)
target_include_directories(plcli PUBLIC include)
target_link_libraries(plcli PUBLIC libpl CLI11)

install(TARGETS plcli DESTINATION bin)