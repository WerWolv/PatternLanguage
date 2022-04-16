cmake_minimum_required(VERSION 3.21)
project(libpl)

set(CMAKE_CXX_STANDARD 20)

add_library(libpl STATIC
        source/helpers/utils.cpp
        source/helpers/file.cpp

        source/pl/evaluator.cpp
        source/pl/lexer.cpp
        source/pl/log_console.cpp
        source/pl/parser.cpp
        source/pl/pattern_language.cpp
        source/pl/preprocessor.cpp
        source/pl/validator.cpp
)

target_include_directories(libpl PUBLIC include)
target_link_libraries(libpl PUBLIC fmt::fmt)

set_target_properties(libpl PROPERTIES PREFIX "")
