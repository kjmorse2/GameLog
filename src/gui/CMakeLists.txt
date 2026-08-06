find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(gamelog-gui STATIC
    MainWindow.cpp
    MainWindow.h
    library/LibraryView.h
    library/LibraryView.cpp
    library/libraryview.ui
        library/RefreshButton.cpp
        library/RefreshButton.h
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
