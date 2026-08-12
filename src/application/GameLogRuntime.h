#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "domain/Session.h"
#include "process/ProcessInfo.h"
#include "process/SteamProcessInspector.h"
#include "services/GameService.h"
#include "services/SessionService.h"

using std::vector;
using std::optional;
using gamelog::core::domain::Game;
using gamelog::core::domain::Session;
using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;


namespace gamelog::core::process {
    class ProcessSource;
}

namespace gamelog::application {

    /**
     * Owns the long-lived resources used by both headless and GUI launch modes.
     * Application operations flow through GameService and SessionService.
     */
    class GameLogRuntime
    {
    public:
        explicit GameLogRuntime(QString databasePath);

        ~GameLogRuntime();

        GameLogRuntime(const GameLogRuntime &) = delete;

        GameLogRuntime &operator=(const GameLogRuntime &) = delete;

        GameLogRuntime(GameLogRuntime &&) = delete;

        GameLogRuntime &operator=(GameLogRuntime &&) = delete;

        /**
         * Starts the runtime.
         * @return true if the runtime started successfully, false otherwise.
         */
        [[nodiscard]] bool start();

        /**
         * Stops the runtime
         */
        void stop();

        /**
         * Updates the runtime.
         * @param elapsed The time that has elapsed since the last update.
         */
        void update(seconds elapsed);

        /**
         * Returns the currently active session, including a restored handoff.
         * @return The active session if one exists.
         */
        [[nodiscard]] optional<Session> activeSession() const;

        /**
         * Refreshes the process-matching cache after library edits.
         * @return a boolean indicating success of reload.
         */
        [[nodiscard]] bool reloadTrackedGames();

        services::GameService *getGameService();

        services::SessionService *getSessionService();

    private:
        // DatabaseManager must outlive every repository and service that uses its
        // QSqlDatabase handle. Members are destroyed in reverse declaration order.

        /**
         * @brief The database manager.
         */
        core::database::DatabaseManager databaseManager_;

        /**
         * @brief interface for the games table.
         */
        optional<core::database::GameRepository> gameRepository_;

        /**
         * @brief interface for the sessions table.
         */
        optional<core::database::SessionRepository> sessionRepository_;
        optional<services::GameService> gameService_;
        optional<services::SessionService> sessionService_;

        /**
         * @brief Libprocs2 based process inspector helper.
         */
        std::unique_ptr<core::process::ProcessSource> processSource_;

        /**
         * @brief Caches and examines steam app ids for running processes.
         */
        core::process::SteamProcessInspector steamProcessInspector_;

        /**
         * @brief A cache of tracked Steam games, loaded from database.
         */
        QHash<std::uint32_t, Game> trackedSteamGames_;

        /**
         * @brief A cache of tracked games by path, loaded from database.
         */
        QHash<QString, Game> trackedPathGames_;

        /**
         * @brief Game struct for active game session, if there is one.
         */
        optional<Game> activeGame_;

        /**
         * @brief The ID of the game that is pending to be started.
         */
        optional<int> pendingGameId_;

        /**
         * @brief Indicates if the runtime is currently running.
         */
        bool running_{false};

        /**
         * @brief Indicates if the database is ready for use.
         */
        bool databaseReady_{false};

        /**
         * @brief Tracks seconds since session started
         */
        seconds gameOpenDuration_{0};

        /**
         * @brief Tracks seconds since game from active session closed.
         */
        seconds gameClosedDuration_{0};

        /**
         * @breif Controls how long a game must be open to start a session.
         */
        static constexpr seconds kStartGracePeriod{30};

        /**
         * @breif Controls how long a game must be closed to end a session.
         */
        static constexpr seconds kEndGracePeriod{30};

        /**
         * Synchronizes the games with the database into the cached QHashes.
         * @return true if successful, false otherwise.
         */
        [[nodiscard]] bool syncGamesWithDatabase();

        /**
         * Reloads an active session from the session manager, and checks with the database for parity.
         * @return A boolean describing if the active session was found.
         */
        [[nodiscard]] bool restoreActiveSession();

        /**
         * Matches a process to the tracked games.
         * @param process The process to chec
         * @return The game, if one is found that matches.
         */
        [[nodiscard]] optional<Game> matchTrackedGame(const ProcessInfo &process) const;

        /**
         * Checks if a process matches a game.
         * @param process The process to check.
         * @param game The game to check.
         * @return A boolean describing if the process matches the game.
         */
        [[nodiscard]] static bool processMatchesGame(const ProcessInfo &process, const Game &game);

        /**
         * Starts a new session for the given game.
         * @param game The game to start a session for.
         * @return A boolean describing if the session was started successfully.
         */
        [[nodiscard]] bool startNewSession(const Game &game);

        /**
         * @brief End the active session.
         * @return Boolean describing success in ending the session.
         */
        [[nodiscard]] bool stopActiveSession();

        /**
         * @brief Reset the pending start state.
         */
        void resetPendingStart() noexcept;
    };
} // namespace gamelog::application
