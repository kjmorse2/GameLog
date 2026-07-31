add_library(gamelog-application STATIC
    GameLogRuntime.cpp
    GameLogRuntime.h
)

target_include_directories(gamelog-application
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

target_link_libraries(gamelog-application
    PUBLIC
        gamelog-core
        Qt6::Core
        Qt6::Sql
)
