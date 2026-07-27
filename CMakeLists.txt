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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Test)

add_library(gamelog-warnings INTERFACE)
target_compile_options(gamelog-warnings
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
        $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
