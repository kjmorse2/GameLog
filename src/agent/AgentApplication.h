#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

#include <QHash>
#include <QString>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "process/ProcessInfo.h"
#include "process/SteamProcessInspector.h"
#include "sessions/SessionManager.h"

namespace gamelog::core::process {
    class ProcessSource;
}

namespace gamelog::agent {

    /**
     * @brief Owns the background agent runtime and process-detection state.
     *
     * Session lifecycle and persistence are delegated to SessionManager.
     */
    class AgentApplication
    {
    public:
        /**
         * @brief Creates the agent around one resolved database path.
         */
        explicit AgentApplication(QString databasePath);

        ~AgentApplication();

        /**
         * @brief Starts process tracking and refreshes the tracked-game cache.
         * @return true when the agent entered the running state.
         */
        [[nodiscard]] bool start();

        /**
         * @brief Stops process tracking and releases the process source.
         */
        void stop();

        /**
         * @brief Scans the current process table for tracked executables.
         * @param elapsed Time since the previous scan.
         */
        void updateAgent(std::chrono::seconds elapsed);

    private:
        // Declare the database owner before every object that holds a copy of its
        // QSqlDatabase handle. Members are destroyed in reverse declaration order,
        // so SessionManager and the repositories disappear before DatabaseManager.
        /**
         * @brief handles database connection
         */
        core::database::DatabaseManager m_databaseManager;

        /**
         * @breif Access the GameTable inside the database
         */
        std::optional<core::database::GameRepository> m_gameRepository;

        /**
         * @breif Access the SessionTable inside the database
         */
        std::optional<core::database::SessionRepository> m_sessionRepository;

        /**
         * @breif Manage and create new active sessions when required
         */
        std::optional<core::sessions::SessionManager> m_sessionManager;


        /**
         * @brief handle gathering of processes using libProc.
         */
        std::unique_ptr<core::process::ProcessSource> m_processSource;

        core::process::SteamProcessInspector m_steamProcessInspector;

        QHash<std::uint32_t, core::domain::Game> m_trackedSteamGames;
        QHash<QString, core::domain::Game> m_trackedPathGames;

        /**
         * @brief Holds the active game being recorded when there is a session being recorded.
         */
        std::optional<core::domain::Game> m_activeGame;

        std::optional<int> m_pendingGameId;

        /**
         * @brief Indicates whether the agent is currently running.
         */
        bool m_running{false};

        /**
         * @brief Indicates whether the database is ready for operations.
         */
        bool m_databaseReady{false};

        /**
         * @brief Tracks the duration for which a game has been open.
         */
        std::chrono::seconds m_gameOpenDuration{0};

        /**
         * @brief Tracks the duration for which a game has been closed.
         */
        std::chrono::seconds m_gameClosedDuration{0};

        // These should eventually come from QSettings. They remain constants here
        // so this refactor does not introduce a settings dependency.
        static constexpr std::chrono::seconds kStartGracePeriod{30};
        static constexpr std::chrono::seconds kEndGracePeriod{30};

        /**
         * @brief Rebuilds the tracked executable set from the current database.
         */
        [[nodiscard]] bool syncGamesWithDatabase();

        /**
         * @brief Resolves a detected process to a tracked game.
         */
        [[nodiscard]] std::optional<core::domain::Game> matchTrackedGame(
                const core::process::ProcessInfo &process) const;

        /**
         * @brief Checks whether a running process still matches a recorded game.
         */
        [[nodiscard]] bool processMatchesGame(
                const core::process::ProcessInfo &process,
                const core::domain::Game &game) const;

        /**
         * @brief Starts a persisted session for a detected game.
         */
        [[nodiscard]] bool startNewSession(const core::domain::Game &game);

        /**
         * @brief Requests that SessionManager persist and complete the active session.
         */
        [[nodiscard]] bool stopActiveSession();

        /**
         * @brief Clears the candidate process and its accumulated start grace time.
         */
        void resetPendingStart() noexcept;
    };

} // namespace gamelog::agent
