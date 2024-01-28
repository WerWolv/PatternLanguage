cmake_minimum_required(VERSION 3.16)
project(plcli)

set(CMAKE_CXX_STANDARD 23)

set(PLCLI_SOURCES
        source/main.cpp

        source/helpers/utils.cpp

        source/subcommands/format.cpp
        source/subcommands/run.cpp
        source/subcommands/docs.cpp
        source/subcommands/info.cpp
)

if (LIBPL_BUILD_CLI_AS_EXECUTABLE)
    add_executable(plcli ${PLCLI_SOURCES})
    target_compile_definitions(plcli PRIVATE LIBPL_CLI_AS_EXECUTABLE)
else ()
    add_library(plcli STATIC ${PLCLI_SOURCES})
    set_property(TARGET plcli PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (NOT USE_SYSTEM_CLI11)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/cli11 ${CMAKE_CURRENT_BINARY_DIR}/../external/cli11)
    set(CLI11_LIBRARIES CLI11::CLI11)
    set(CLI11_INCLUDE_DIRS)
else()
    find_package(CLI11 CONFIG QUIET)
endif()

target_compile_options(plcli PRIVATE -Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-array-bounds)
target_include_directories(plcli PUBLIC include ${CLI11_INCLUDE_DIRS})
target_link_libraries(plcli PRIVATE ${CLI11_LIBRARIES} ${NLOHMANN_JSON_LIBRARIES} libpl libpl-gen ${FMT_LIBRARIES})

if (WIN32)
    set(CMAKE_INSTALL_BINDIR ".")

    install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
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
