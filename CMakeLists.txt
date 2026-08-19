cmake_minimum_required(VERSION 3.24)

project(GameLog VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Coverage
# ---------------------------------------------------------------------------

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if (ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")

        add_compile_options(
                -O0
                -g
                --coverage
        )

        add_link_options(
                --coverage
        )
    else ()
        message(WARNING "Coverage requested, but compiler is not GCC/Clang")
    endif ()
endif ()

include(CTest)
include(GNUInstallDirs)

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
    configure_file(
            packaging/applications/gamelog.desktop.in
            ${CMAKE_CURRENT_BINARY_DIR}/gamelog.desktop
            @ONLY
    )

    configure_file(
            packaging/systemd/gamelog-headless.service.in
            ${CMAKE_CURRENT_BINARY_DIR}/gamelog-headless.service
            @ONLY
    )

    install(
            TARGETS gamelog
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
            PROGRAMS packaging/scripts/gamelog-gui-launcher
            DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
            FILES ${CMAKE_CURRENT_BINARY_DIR}/gamelog-headless.service
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user
    )

    install(
            FILES ${CMAKE_CURRENT_BINARY_DIR}/gamelog.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
endif ()