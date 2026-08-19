# ---------------------------------------------------------------------------
# Shared test support
# ---------------------------------------------------------------------------

add_library(gamelog-test-support STATIC
        fixtures/TestDatabaseFixture.cpp
        fixtures/FakeNetworkAccessManager.cpp
        fixtures/FakeProcessSource.h
        fixtures/LoggingTestSupport.cpp
)

target_include_directories(gamelog-test-support
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamelog-test-support
        PUBLIC
        gamelog-core
        Qt6::Core
        Qt6::Network

        PRIVATE
        gamelog-warnings
)

# ---------------------------------------------------------------------------
# Grouping targets
# ---------------------------------------------------------------------------

add_custom_target(gamelog-all-tests-build)
add_custom_target(gamelog-database-tests-build)


# ---------------------------------------------------------------------------
# General QTest helper
# ---------------------------------------------------------------------------

function(add_gamelog_test target_name source_file labels)
    add_executable(
            ${target_name}
            ${source_file}
    )

    target_link_libraries(
            ${target_name}
            PRIVATE
            gamelog-core
            Qt6::Core
            Qt6::Test
            gamelog-warnings
    )

    add_test(
            NAME ${target_name}
            COMMAND ${target_name}
    )

    set_tests_properties(
            ${target_name}
            PROPERTIES
            LABELS "${labels}"
    )

    add_dependencies(
            gamelog-all-tests-build
            ${target_name}
    )
endfunction()


# ---------------------------------------------------------------------------
# Application QTest helper
# ---------------------------------------------------------------------------

function(add_gamelog_application_test target_name source_file labels)
    add_gamelog_test(
            ${target_name}
            ${source_file}
            "${labels}"
    )

    target_link_libraries(
            ${target_name}
            PRIVATE
            gamelog-test-support
            gamelog-application
            Qt6::Sql
    )
endfunction()


# ---------------------------------------------------------------------------
# Database QTest helper
# ---------------------------------------------------------------------------

function(add_gamelog_database_test target_name source_file)
    add_gamelog_application_test(
            ${target_name}
            ${source_file}
            "database;integration"
    )

    add_dependencies(
            gamelog-database-tests-build
            ${target_name}
    )
endfunction()


# ---------------------------------------------------------------------------
# Database tests
# ---------------------------------------------------------------------------

add_gamelog_database_test(
        gamelog-database-manager-test
        core/database/DatabaseManagerTest.cpp
)

add_gamelog_database_test(
        gamelog-game-service-test
        application/services/local/GameServiceTest.cpp
)

add_gamelog_database_test(
        gamelog-session-service-test
        application/services/local/SessionServiceTest.cpp
)

add_gamelog_database_test(
        gamelog-game-repository-test
        core/database/GameRepositoryTest.cpp
)

add_gamelog_database_test(
        gamelog-session-repository-test
        core/database/SessionRepositoryTest.cpp
)

add_gamelog_database_test(
        gamelog-database-migrator-test
        core/database/DatabaseMigratorTest.cpp
)


# ---------------------------------------------------------------------------
# Application tests
# ---------------------------------------------------------------------------

add_gamelog_application_test(
        gamelog-run-mode-test
        application/RunModeTest.cpp
        "unit;application"
)

add_gamelog_application_test(
        gamelog-runtime-test
        application/GameLogRuntimeTest.cpp
        "application;integration"
)

add_gamelog_application_test(
        gamelog-credential-service-test
        application/services/local/CredentialServiceTest.cpp
        "unit;application"
)

add_gamelog_application_test(
        gamelog-steam-api-service-test
        application/services/web/SteamApiServiceTest.cpp
        "unit;application"
)

add_gamelog_application_test(
        gamelog-game-artwork-service-test
        application/services/web/GameArtworkServiceTest.cpp
        "unit;application"
)


# ---------------------------------------------------------------------------
# General domain tests
# ---------------------------------------------------------------------------

add_gamelog_test(
        gamelog-core-smoke-test
        CoreSmokeTest.cpp
        "unit;core;domain"
)

add_gamelog_test(
        gamelog-session-test
        core/domain/SessionTest.cpp
        "unit;core;domain"
)

add_gamelog_test(
        gamelog-process-test
        core/process/ProcessTest.cpp
        "unit;core;domain"
)

add_gamelog_test(
        gamelog-process-helpers-test
        core/process/ProcessHelpersTest.cpp
        "unit;core;process"
)

add_gamelog_test(
        gamelog-steam-process-inspector-test
        core/process/SteamProcessInspectorTest.cpp
        "unit;core;process"
)

add_gamelog_test(
        gamelog-app-paths-test
        core/resources/AppPathsTest.cpp
        "unit;core;resources"
)