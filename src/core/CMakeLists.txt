add_library(gamelog-core STATIC
        database/DatabaseManager.cpp
        database/DatabaseMigrator.cpp
        database/GameRepository.cpp
        database/SessionRepository.cpp

        logging/LoggingCategories.cpp
        domain/Session.cpp
        domain/query/GameQuery.h
        domain/query/GameQuery.cpp
        domain/query/SessionQuery.h
        domain/query/SessionQuery.cpp
        domain/query/QueryOptions.h
        domain/query/QueryOptions.cpp
        process/ProcfsProcessSource.cpp
        process/ProcessHelpers.cpp
        process/SteamProcessInspector.cpp
        process/SteamProcessInspector.h

        ${PROJECT_SOURCE_DIR}/resources/migrations/migrations.qrc
        domain/Game.cpp
        resources/AppPaths.cpp
        resources/AppPaths.h
)

target_include_directories(gamelog-core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamelog-core
        PUBLIC
        Qt6::Core
        Qt6::Sql

        PRIVATE
        PkgConfig::PROC2
        gamelog-warnings
)