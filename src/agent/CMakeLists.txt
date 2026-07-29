add_executable(gamelog-agent
        main.cpp
        AgentApplication.cpp
)

target_link_libraries(gamelog-agent
        PRIVATE
        gamelog-core
        Qt6::Core
        gamelog-warnings
)

install(TARGETS gamelog-agent RUNTIME DESTINATION bin)
