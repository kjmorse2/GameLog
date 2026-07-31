add_executable(gamelog-gui
        main.cpp
        main_window/MainWindow.cpp
        library_view/LibraryView.cpp
        library_view/GameCard.cpp
        library_view/GameCard.h
        library_view/GameCard.ui
)

target_include_directories(gamelog-gui
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamelog-gui
        PRIVATE
        gamelog-core
        Qt6::Core
        Qt6::Widgets
        gamelog-warnings
)

install(TARGETS gamelog-gui RUNTIME DESTINATION bin)
