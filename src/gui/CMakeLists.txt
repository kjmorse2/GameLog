find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(gamelog-gui STATIC
    MainWindow.cpp
    MainWindow.h
    mainwindow.ui
    library/LibraryView.h
    library/LibraryView.cpp
    library/libraryview.ui
    calander/CalanderView.cpp
    calander/CalanderView.h
        calander/calanderview.ui
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
