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
#include "process/ProcessInfo.h"
#include "process/SteamProcessInspector.h"
#include "sessions/SessionManager.h"

namespace gamelog::core::process {
class ProcessSource;
}

namespace gamelog::application {

/**
 * Owns the long-lived database, session, and process-tracking state used by
 * both --headless and --gui modes.
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

    [[nodiscard]] bool start();
    void stop();
    void update(std::chrono::seconds elapsed);

    /** Direct in-process query used by the GUI. */
    [[nodiscard]] std::vector<core::domain::Game> listGames() const;

    /** Returns the currently active session, including a restored handoff. */
    [[nodiscard]] std::optional<core::domain::Session> activeSession() const;

    /** Refreshes the process-matching cache after library edits. */
    [[nodiscard]] bool reloadTrackedGames();

private:
    // DatabaseManager must outlive every repository and service that uses its
    // QSqlDatabase handle. Members are destroyed in reverse declaration order.
    core::database::DatabaseManager databaseManager_;
    std::optional<core::database::GameRepository> gameRepository_;
    std::optional<core::database::SessionRepository> sessionRepository_;
    std::optional<core::sessions::SessionManager> sessionManager_;

    std::unique_ptr<core::process::ProcessSource> processSource_;
    core::process::SteamProcessInspector steamProcessInspector_;

    QHash<std::uint32_t, core::domain::Game> trackedSteamGames_;
    QHash<QString, core::domain::Game> trackedPathGames_;

    std::optional<core::domain::Game> activeGame_;
    std::optional<int> pendingGameId_;

    bool running_{false};
    bool databaseReady_{false};

    std::chrono::seconds gameOpenDuration_{0};
    std::chrono::seconds gameClosedDuration_{0};

    static constexpr std::chrono::seconds kStartGracePeriod{30};
    static constexpr std::chrono::seconds kEndGracePeriod{30};

    [[nodiscard]] bool syncGamesWithDatabase();
    [[nodiscard]] bool restoreActiveSession();

    [[nodiscard]] std::optional<core::domain::Game> matchTrackedGame(
        const core::process::ProcessInfo &process) const;

    [[nodiscard]] bool processMatchesGame(
        const core::process::ProcessInfo &process,
        const core::domain::Game &game) const;

    [[nodiscard]] bool startNewSession(const core::domain::Game &game);
    [[nodiscard]] bool stopActiveSession();
    void resetPendingStart() noexcept;
};

} // namespace gamelog::application
