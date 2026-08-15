#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "process/SteamProcessInspector.h"
#include "services/GameService.h"
#include "services/GameArtworkService.h"
#include "services/SessionService.h"

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
    class GameLogRuntime
    {
    public:
        explicit GameLogRuntime(QString databasePath);

        ~GameLogRuntime();

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
         */
        [[nodiscard]] services::GameService* getGameService() noexcept;

        /**
         * Returns the owned session service, or nullptr if database initialization failed.
         */
        [[nodiscard]] services::SessionService* getSessionService() noexcept;
        [[nodiscard]] services::GameArtworkService* getArtworkService() noexcept;

    private:
        // DatabaseManager must outlive every repository and service that uses its
        // QSqlDatabase handle. Members are destroyed in reverse declaration order.
        core::database::DatabaseManager databaseManager_;
        std::optional<core::database::GameRepository> gameRepository_;
        std::optional<core::database::SessionRepository> sessionRepository_;
        std::optional<services::GameService> gameService_;
        std::optional<services::SessionService> sessionService_;
        std::optional<services::GameArtworkService> gameArtworkService_;

        std::unique_ptr<core::process::ProcessSource> processSource_;
        core::process::SteamProcessInspector steamProcessInspector_;

        bool running_{false};
        bool databaseReady_{false};
    };
} // namespace gamelog::application
