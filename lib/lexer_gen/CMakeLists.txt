set(SOURCES
    ../source/pl/core/new_lexer_sm.cpp
    main.cpp
)

add_executable(lexer_gen ${SOURCES})

target_include_directories(lexer_gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_include_directories(lexer_gen PRIVATE include ../../external/lexertl17/include)
