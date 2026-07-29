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

add_test(NAME gamelog-core-smoke COMMAND gamelog-core-smoke)

add_executable(gamelog-database-manager-test
        fixtures/TestDatabaseFixture.cpp
        unit/core/database/DatabaseManagerTest.cpp
)

target_link_libraries(gamelog-database-manager-test
        PRIVATE
        gamelog-core
        Qt6::Core
        Qt6::Sql
        Qt6::Test
        gamelog-warnings
)

target_include_directories(gamelog-database-manager-test
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME gamelog-database-manager-test COMMAND gamelog-database-manager-test)

add_executable(gamelog-game-repository-test
        fixtures/TestDatabaseFixture.cpp
        unit/core/database/GameRepositoryTest.cpp
)

target_link_libraries(gamelog-game-repository-test
        PRIVATE
        gamelog-core
        Qt6::Core
        Qt6::Sql
        Qt6::Test
        gamelog-warnings
)

target_include_directories(gamelog-game-repository-test
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME gamelog-game-repository-test COMMAND gamelog-game-repository-test)

add_executable(gamelog-session-repository-test
        fixtures/TestDatabaseFixture.cpp
        unit/core/database/SessionRepositoryTest.cpp
)

target_link_libraries(gamelog-session-repository-test
        PRIVATE
        gamelog-core
        Qt6::Core
        Qt6::Sql
        Qt6::Test
        gamelog-warnings
)

target_include_directories(gamelog-session-repository-test
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME gamelog-session-repository-test COMMAND gamelog-session-repository-test)