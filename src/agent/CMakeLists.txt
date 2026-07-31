add_library(gamelog-agent-lib
        STATIC
        AgentApplication.cpp
        AgentIpcServer.cpp
        ${PROJECT_SOURCE_DIR}/resources/migrations/migrations.qrc
)

target_link_libraries(gamelog-agent-lib
        PUBLIC
        gamelog-core
        Qt6::Core
        Qt6::Network
        gamelog-warnings
)

add_executable(gamelog-agent
        main.cpp
)

target_link_libraries(gamelog-agent
        PRIVATE
        gamelog-agent-lib
)
target_include_directories(gamelog-agent-lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
install(TARGETS gamelog-agent RUNTIME DESTINATION bin)
