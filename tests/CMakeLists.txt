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
