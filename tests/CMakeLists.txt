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
        Qt6::Sql

        PRIVATE
        gamelog-warnings
)


# ---------------------------------------------------------------------------
# Grouping targets
#
# These do not create executables. They collect dependencies so the custom
# test-running targets can ensure every required executable is built first.
# ---------------------------------------------------------------------------

add_custom_target(gamelog-all-tests-build)
add_custom_target(gamelog-database-tests-build)


# ---------------------------------------------------------------------------
# Helper for database QTest executables
# ---------------------------------------------------------------------------

function(add_gamelog_database_test target_name source_file)
    add_executable(${target_name}
            ${source_file}
    )

    target_link_libraries(${target_name}
            PRIVATE
            gamelog-test-support
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
            LABELS "database;integration"
    )

    add_dependencies(
            gamelog-all-tests-build
            ${target_name}
    )

    add_dependencies(
            gamelog-database-tests-build
            ${target_name}
    )
endfunction()


# ---------------------------------------------------------------------------
# Smoke test
# ---------------------------------------------------------------------------

add_executable(gamelog-core-smoke
        unit/CoreSmokeTest.cpp
)

target_link_libraries(gamelog-core-smoke
        PRIVATE
        gamelog-core
        Qt6::Core
        Qt6::Test
        gamelog-warnings
)

add_test(
        NAME gamelog-core-smoke
        COMMAND gamelog-core-smoke
)

set_tests_properties(
        gamelog-core-smoke
        PROPERTIES
        LABELS "unit;smoke"
)

add_dependencies(
        gamelog-all-tests-build
        gamelog-core-smoke
)


# ---------------------------------------------------------------------------
# Database test executables
# ---------------------------------------------------------------------------

add_gamelog_database_test(
        gamelog-database-manager-test
        integration/database/DatabaseManagerTest.cpp
)

add_gamelog_database_test(
        gamelog-game-repository-test
        integration/database/GameRepositoryTest.cpp
)

add_gamelog_database_test(
        gamelog-session-repository-test
        integration/database/SessionRepositoryTest.cpp
)


# ---------------------------------------------------------------------------
# Convenient commands
# ---------------------------------------------------------------------------

add_custom_target(check
        COMMAND
        ${CMAKE_CTEST_COMMAND}
        --output-on-failure

        WORKING_DIRECTORY
        ${CMAKE_BINARY_DIR}

        COMMENT
        "Running the complete GameLog test suite"

        USES_TERMINAL
)

add_dependencies(
        check
        gamelog-all-tests-build
)


add_custom_target(check-database
        COMMAND
        ${CMAKE_CTEST_COMMAND}
        --output-on-failure
        -L database

        WORKING_DIRECTORY
        ${CMAKE_BINARY_DIR}

        COMMENT
        "Running GameLog database tests"

        USES_TERMINAL
)

add_dependencies(
        check-database
        gamelog-database-tests-build
)