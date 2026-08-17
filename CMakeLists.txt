cmake_minimum_required(VERSION 3.24)

project(GameLog VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql Test Keychain)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PROC2 REQUIRED IMPORTED_TARGET libproc2)

add_library(gamelog-warnings INTERFACE)

target_compile_options(gamelog-warnings
        INTERFACE
        $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
        $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

option(
        GAMELOG_ENABLE_INSTALL
        "Generate GameLog installation rules"
        OFF
)

if (GAMELOG_ENABLE_INSTALL)
    install(TARGETS gamelog RUNTIME DESTINATION bin)

    install(PROGRAMS
            packaging/scripts/gamelog-gui-launcher
            DESTINATION bin
    )

    install(FILES
            packaging/systemd/gamelog-headless.service
            DESTINATION lib/systemd/user
    )

    install(FILES
            packaging/applications/gamelog.desktop
            DESTINATION share/applications
    )
endif ()