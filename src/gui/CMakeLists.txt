add_library(gamelog-gui STATIC
    MainWindow.cpp
    MainWindow.h
)

target_include_directories(gamelog-gui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(gamelog-gui
    PUBLIC
        gamelog-application
        Qt6::Widgets
)
