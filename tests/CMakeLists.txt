# ---------------------------------------------------------------------------
# Shared test support
# ---------------------------------------------------------------------------

add_library(gamelog-test-support STATIC
        fixtures/TestDatabaseFixture.cpp
)

target_include_directories(gamelog-test-support
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamelog-test-support
        PUBLIC
        gamelog-core
        Qt6::Core

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
# Database QTest helper
# ---------------------------------------------------------------------------

function(add_gamelog_database_test target_name source_file)
    add_gamelog_test(
            ${target_name}
            ${source_file}
            "database;integration"
    )

    target_link_libraries(
            ${target_name}
            PRIVATE
            gamelog-test-support
            gamelog-application
            Qt6::Sql
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
        integration/database/DatabaseManagerTest.cpp
)

add_gamelog_database_test(
        gamelog-game-service-test
        integration/services/GameServiceTest.cpp
)

add_gamelog_database_test(
        gamelog-session-service-test
        integration/services/SessionServiceTest.cpp
)


# ---------------------------------------------------------------------------
# General domain tests
# ---------------------------------------------------------------------------

add_gamelog_test(
        gamelog-session-test
        unit/core/domain/SessionTest.cpp
        "unit;core;domain"
)

add_gamelog_test(
        gamelog-process-test
        unit/core/process/ProcessTest.cpp
        "unit;core;domain"
)