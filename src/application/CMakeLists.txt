add_library(gamelog-application STATIC
        GameLogRuntime.cpp
        GameLogRuntime.h
        RunMode.cpp
        RunMode.h
        services/local/GameService.h
        services/local/GameService.cpp
        services/local/SessionService.h
        services/local/SessionService.cpp
        services/web/GameArtworkService.h
        services/web/GameArtworkService.cpp
        services/web/SteamApiService.cpp
        services/web/SteamApiService.h
        services/local/CredentialService.cpp
        services/local/CredentialService.h
)

target_include_directories(gamelog-application
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

target_link_libraries(gamelog-application
        PUBLIC
        gamelog-core
        Qt6::Core
        Qt6::Network
        Qt6::Sql
        Qt6Keychain::Qt6Keychain
        Qt6::Gui
)