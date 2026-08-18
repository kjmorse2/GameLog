#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "process/SteamProcessInspector.h"
#include "services/local/CredentialService.h"
#include "services/local/GameService.h"
#include "services/web/GameArtworkService.h"
#include "services/web/SteamApiService.h"
#include "services/local/SessionService.h"

namespace gamelog::core::process
{
    class ProcessSource;
}

namespace gamelog::application
{
    /**
     * Owns the long-lived resources used by both headless and GUI launch modes.
     * Application operations flow through GameService and SessionService.
     */
    class GameLogRuntime : public QObject
    {
        Q_OBJECT

    public:
        explicit GameLogRuntime(QString databasePath);

        ~GameLogRuntime() override;

        GameLogRuntime(const GameLogRuntime&) = delete;

        GameLogRuntime& operator=(const GameLogRuntime&) = delete;

        GameLogRuntime(GameLogRuntime&&) = delete;

        GameLogRuntime& operator=(GameLogRuntime&&) = delete;

        /**
         * Starts process monitoring and restores service state.
         * @return true if the runtime started successfully, false otherwise.
         */
        [[nodiscard]] bool start();

        /**
         * Stops process monitoring without completing an active session.
         */
        void stop();

        /**
         * Polls the process source and delegates session tracking to SessionService.
         * @param elapsed The time that has elapsed since the last update.
         */
        void update(std::chrono::seconds elapsed);

        /**
         * Returns the owned game service, or nullptr if database initialization failed.
         * @return The GameService instance, or nullptr if the database is not ready.
         */
        [[nodiscard]] services::GameService* getGameService() noexcept;

        /**
         * Returns the owned session service, or nullptr if database initialization failed.
         * @return The SessionService instance, or nullptr if the database is not ready.
         */
        [[nodiscard]] services::SessionService* getSessionService() noexcept;

        /**
         * Returns the owned artwork service, or nullptr if database initialization failed.
         * @return The GameArtworkService instance, or nullptr if the database is not ready.
         */
        [[nodiscard]] services::GameArtworkService* getArtworkService() noexcept;

        /**
         * Returns the owned credential service, or nullptr if database initialization failed.
         * @return The CredentialService instance, or nullptr if the database is not ready.
         */
        [[nodiscard]] services::CredentialService* getCredentialService() noexcept;

    private:
        /**
         * @brief Manages the retrieval of sensitive credentials, such as the Steam API key and player ID,
         * which are required for certain operations. This service is optional and only initialized if the database is ready.
         */
        std::optional<services::CredentialService> credentialService_;

        /**
         * Depends on the CredentialService for Steam API operations and the GameRepository for game data.
         */
        std::optional<services::SteamApiService> steamApiService_;

        /**
         * @brief The DatabaseManager instance responsible for managing the database connection and schema, initialized
         * before repos to ensure dependencies are available for the repositories and services that follow.
         */
        core::database::DatabaseManager databaseManager_;

        /**
         * @brief The GameRepository instance responsible for managing game data in the database.
         * This repository is optional and only initialized if the database is ready.
         */
        std::optional<core::database::GameRepository> gameRepository_;

        /**
         * @brief The SessionRepository instance responsible for managing session data in the database.j
         */
        std::optional<core::database::SessionRepository> sessionRepository_;

        /**
         * @brief The GameService instance responsible for application-facing game operations,
         * including querying and managing games. Dependent on the GameRepository and SteamApiService,
         * this service is optional and only initialized if the database is ready.
         */
        std::optional<services::GameService> gameService_;

        /**
         * @brief The SessionService instance responsible for managing game sessions, including tracking and updating session data.
         * Dependent on the SessionRepository and GameService, this service is optional and only initialized
         */
        std::optional<services::SessionService> sessionService_;

        /**
         * @brief The GameArtworkService instance responsible for managing game artwork, including downloading and storing artwork.
         */
        std::optional<services::GameArtworkService> gameArtworkService_;

        /**
         * @brief The ProcessSource instance responsible for providing a list of currently running processes on the system.
         */
        std::unique_ptr<core::process::ProcessSource> processSource_;

        /**
         * @brief The SteamProcessInspector instance responsible for annotating processes with Steam-related information,
         * such as identifying which processes correspond to tracked Steam games. This inspector is used during the
         * update() method to enhance the process list with Steam-specific data.
         */
        core::process::SteamProcessInspector steamProcessInspector_;

        /**
         * @brief Indicates whether the GameLogRuntime is currently running. This flag is used to prevent multiple
         * starts and to ensure that update() is only called when the runtime is active.
         */
        bool running_{false};

        /**
         * @brief Indicates whether the database and all required services are ready for use. This flag is set to true
         * after successful initialization of the DatabaseManager, GameRepository, SessionRepository, and all dependent
         * services. If the database is not ready, the runtime will not start, and service accessors will return nullptr.
         */
        bool databaseReady_{false};
    };
} // namespace gamelog::application
