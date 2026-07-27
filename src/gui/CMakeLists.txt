add_executable(gamelog
    main.cpp
    application/GuiApplication.cpp
    main_window/MainWindow.cpp
    tray/TrayController.cpp
    active_session/ActiveSessionPanel.cpp
    notes/SessionNoteEditor.cpp
)

target_include_directories(gamelog
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamelog
    PRIVATE
        gamelog-core
        Qt6::Widgets
        gamelog-warnings
)

install(TARGETS gamelog RUNTIME DESTINATION bin)
