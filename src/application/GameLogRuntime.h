#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QHash>
#include <QString>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "domain/Session.h"
#include "domain/query/GameQuery.h"
#include "domain/query/SessionQuery.h"
#include "process/ProcessInfo.h"
#include "process/SteamProcessInspector.h"
#include "services/GameService.h"
#include "services/SessionService.h"

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
    void update(std::chrono::seconds elapsed);

    [[nodiscard]] std::vector<core::domain::Game> searchGames(
        const core::domain::query::GameQuery &query) const;
    [[nodiscard]] std::vector<core::domain::Session> searchSessions(
        const core::domain::query::SessionQuery &query) const;
    [[nodiscard]] std::vector<core::domain::Game> listGames() const;

    /**
     * Returns the currently active session, including a restored handoff.
     * @return The active session if one exists.
     */
    [[nodiscard]] std::optional<core::domain::Session> activeSession() const;

    /**
     * Refreshes the process-matching cache after library edits.
     * @return a boolean indicating success of reload.
     */
    [[nodiscard]] bool reloadTrackedGames();

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
    std::optional<core::database::GameRepository> gameRepository_;

    /**
     * @brief interface for the sessions table.
     */
    std::optional<core::database::SessionRepository> sessionRepository_;
    std::optional<services::GameService> gameService_;
    std::optional<services::SessionService> sessionService_;

    /**
     * @brief interface for the session manager.
     */
    // std::optional<core::sessions::SessionManager> sessionManager_;

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
    QHash<std::uint32_t, core::domain::Game> trackedSteamGames_;

    /**
     * @brief A cache of tracked games by path, loaded from database.
     */
    QHash<QString, core::domain::Game> trackedPathGames_;

    /**
     * @brief Game struct for active game session, if there is one.
     */
    std::optional<core::domain::Game> activeGame_;

    /**
     * @brief The ID of the game that is pending to be started.
     */
    std::optional<int> pendingGameId_;

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
    std::chrono::seconds gameOpenDuration_{0};

    /**
     * @brief Tracks seconds since game from active session closed.
     */
    std::chrono::seconds gameClosedDuration_{0};

    /**
     * @breif Controls how long a game must be open to start a session.
     */
    static constexpr std::chrono::seconds kStartGracePeriod{30};

    /**
     * @breif Controls how long a game must be closed to end a session.
     */
    static constexpr std::chrono::seconds kEndGracePeriod{30};

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
    [[nodiscard]] std::optional<core::domain::Game>
    matchTrackedGame(const core::process::ProcessInfo &process) const;
    [[nodiscard]] bool processMatchesGame(
        const core::process::ProcessInfo &process,
        const core::domain::Game &game) const;
    [[nodiscard]] bool startNewSession(const core::domain::Game &game);

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
