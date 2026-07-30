add_library(gamelog-core STATIC
        database/DatabaseManager.cpp
        database/DatabaseMigrator.cpp
        database/GameRepository.cpp
        database/SessionRepository.cpp

        sessions/SessionManager.cpp
        logging/LoggingCategories.cpp
        domain/Session.cpp
        process/ProcfsProcessSource.cpp
        process/ProcessHelpers.cpp
        process/SteamProcessInspector.cpp
        process/SteamProcessInspector.h

        ${PROJECT_SOURCE_DIR}/resources/migrations/migrations.qrc
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