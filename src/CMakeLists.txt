add_subdirectory(core)
add_subdirectory(application)
add_subdirectory(gui)

add_executable(gamelog
    main.cpp
)

target_link_libraries(gamelog
    PRIVATE
        gamelog-application
        gamelog-gui
        Qt6::Core
        Qt6::Widgets
)
