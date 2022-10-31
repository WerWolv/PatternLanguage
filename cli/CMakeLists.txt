cmake_minimum_required(VERSION 3.16)
project(plcli)

set(CMAKE_CXX_STANDARD 20)

add_executable(plcli
        source/main.cpp

        source/helpers/utils.cpp

        source/subcommands/format.cpp
)

add_subdirectory(../external/cli11 ../external/cli11)

target_compile_options(plcli PRIVATE -Wall -Wextra -Werror -Wno-unknown-pragmas)
target_include_directories(plcli PUBLIC include)
target_link_libraries(plcli PUBLIC libpl libpl-gen CLI11)

if (WIN32)
    set(CMAKE_INSTALL_BINDIR ".")

    INSTALL(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
    INSTALL(CODE "get_filename_component(PY_PARENT \"${Python_LIBRARIES}\" DIRECTORY)")
    INSTALL(CODE "LIST(APPEND DEP_FOLDERS \${PY_PARENT})")
    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES $<TARGET_FILE:plcli>
            RESOLVED_DEPENDENCIES_VAR _r_deps
            UNRESOLVED_DEPENDENCIES_VAR _u_deps
            CONFLICTING_DEPENDENCIES_PREFIX _c_deps
            DIRECTORIES ${DEP_FOLDERS} $ENV{PATH}
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )

        if(_u_deps)
            message(WARNING "There were unresolved dependencies for binary: \"${_u_deps}\"!")
        endif()
        if(_c_deps_FILENAMES)
            message(WARNING "There were conflicting dependencies for library: \"${_c_deps}\"!")
        endif()

        foreach(_file ${_r_deps})
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${_file}"
                )
        endforeach()
        ]])

else ()
    set(CMAKE_INSTALL_BINDIR "bin")
endif ()

install(TARGETS plcli DESTINATION ${CMAKE_INSTALL_BINDIR})
