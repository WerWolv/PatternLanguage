set(SOURCES
    main.cpp
)

add_executable(lexer_gen ${SOURCES})

target_include_directories(lexer_gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
