find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(gamelog-gui STATIC
        MainWindow.cpp
        MainWindow.h
        mainwindow.ui
        library/LibraryView.h
        library/LibraryView.cpp
        library/libraryview.ui
        game_card/GameCard.cpp
        game_card/GameCard.h
        game_card/gamecard.ui
        calendar/CalendarView.cpp
        calendar/CalendarView.h
        calendar/calendarview.ui
        live_window/LiveWindow.cpp
        live_window/LiveWindow.h
        live_window/livewindow.ui
        text_editor/TextEditor.cpp
        text_editor/TextEditor.h
        text_editor/texteditor.ui
        ${PROJECT_SOURCE_DIR}/resources/images/images.qrc
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
