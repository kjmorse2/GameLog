#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include <QObject>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "process/SteamProcessInspector.h"
#include "services/local/CredentialService.h"
#include "services/local/GameService.h"
#include "services/local/SessionService.h"
#include "services/web/GameArtworkService.h"
#include "services/web/SteamApiService.h"

namespace gamelog::core::process
{
    class ProcessSource;
}

namespace gamelog::application
{
    /**
     * Owns the long-lived resources used by both headless and GUI launch modes.
     * Application operations flow through GameService and SessionService.
     *
     * The fixed database connection name intentionally permits one live runtime
     * per process. A runtime instance may nevertheless be started again after
     * stop() has completed.
     */
    class GameLogRuntime : public QObject
    {
        Q_OBJECT

    public:
        using ProcessSourceFactory = std::function<std::unique_ptr<core::process::ProcessSource>()>;

        explicit GameLogRuntime(QString databasePath);

        /**
         * Creates a runtime with deterministic process dependencies for tests.
         * Production callers use the single-argument constructor.
         * @param databasePath SQLite database path.
         * @param processSourceFactory Factory used on every successful start attempt.
         * @param steamAppIdReader Reader used by SteamProcessInspector.
         */
        GameLogRuntime(QString databasePath,
                       ProcessSourceFactory processSourceFactory,
                       core::process::SteamProcessInspector::SteamAppIdReader steamAppIdReader);

        ~GameLogRuntime() override;

        GameLogRuntime(const GameLogRuntime&) = delete;
        GameLogRuntime& operator=(const GameLogRuntime&) = delete;
        GameLogRuntime(GameLogRuntime&&) = delete;
        GameLogRuntime& operator=(GameLogRuntime&&) = delete;

        /**
         * Starts process monitoring and restores service state. Starting an
         * already-running instance is rejected; starting again after stop() is supported.
         * @return True if the runtime started successfully.
         */
        [[nodiscard]] bool start();

        /**
         * Stops process monitoring without completing an active session.
         */
        void stop();

        /**
         * Polls the process source and delegates session tracking to SessionService.
         * @param elapsed The time elapsed since the prior update.
         */
        void update(std::chrono::seconds elapsed);

        /**
         * Returns the owned game service, or nullptr if database initialization failed.
         */
        [[nodiscard]] services::GameService* getGameService() noexcept;

        /**
         * Returns the owned session service, or nullptr if database initialization failed.
         */
        [[nodiscard]] services::SessionService* getSessionService() noexcept;

        /**
         * Returns the owned artwork service, or nullptr if database initialization failed.
         */
        [[nodiscard]] services::GameArtworkService* getArtworkService() noexcept;

        /**
         * Returns the owned credential service, or nullptr if database initialization failed.
         */
        [[nodiscard]] services::CredentialService* getCredentialService() noexcept;

    private:
        /**
         * @brief Owns the SQLite connection and applies schema migrations before dependent objects are created.
         */
        core::database::DatabaseManager databaseManager_;

        /**
         * @brief Repository responsible for persisted game data.
         */
        std::optional<core::database::GameRepository> gameRepository_;

        /**
         * @brief Repository responsible for persisted session and note data.
         */
        std::optional<core::database::SessionRepository> sessionRepository_;

        /**
         * @brief Service responsible for retrieving sensitive credentials.
         */
        std::optional<services::CredentialService> credentialService_;

        /**
         * @brief Service responsible for Steam Web API requests.
         */
        std::optional<services::SteamApiService> steamApiService_;

        /**
         * @brief Application-facing game operations and process indexes.
         */
        std::optional<services::GameService> gameService_;

        /**
         * @brief Application-facing session operations and lifecycle state.
         */
        std::optional<services::SessionService> sessionService_;

        /**
         * @brief Service responsible for local and downloaded game artwork.
         */
        std::optional<services::GameArtworkService> gameArtworkService_;

        /**
         * @brief Factory used to recreate the process source on each start.
         */
        ProcessSourceFactory processSourceFactory_;

        /**
         * @brief Current process source while the runtime is running.
         */
        std::unique_ptr<core::process::ProcessSource> processSource_;

        /**
         * @brief Adds cached Steam environment identity to process snapshots.
         */
        core::process::SteamProcessInspector steamProcessInspector_;

        /**
         * @brief True while process polling is active.
         */
        bool running_{false};

        /**
         * @brief True when the database and all required services were initialized.
         */
        bool databaseReady_{false};
    };
} // namespace gamelog::application
