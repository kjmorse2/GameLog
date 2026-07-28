add_library(gamelog-core STATIC
    database/DatabaseManager.cpp
    sessions/SessionManager.cpp
    steam/SteamLibraryScanner.cpp
    settings/ApplicationSettings.cpp
    logging/LoggingCategories.cpp
    ipc/IpcStubs.cpp
    domain/Session.cpp
    process/ProcfsProcessSource.cpp
)

target_include_directories(gamelog-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamelog-core
    PUBLIC
        Qt6::Core
        Qt6::Sql
        Qt6::Network

    PRIVATE
        PkgConfig::PROC2
        gamelog-warnings
)