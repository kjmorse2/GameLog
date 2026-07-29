#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QSet>
#include <QString>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "process/ProcessInfo.h"
#include "process/ProcessSource.h"
#include "sessions/SessionManager.h"

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
        core::database::DatabaseManager m_databaseManager;
        std::optional<core::database::GameRepository> m_gameRepository;
        std::optional<core::database::SessionRepository> m_sessionRepository;
        std::optional<core::sessions::SessionManager> m_sessionManager;

        std::unique_ptr<core::process::ProcessSource> m_processSource;
        QSet<QString> m_trackedExecutablePaths;

        // An optional expresses the state directly: no value means no game is
        // currently being tracked. This replaces a separate m_recording flag plus
        // a default-constructed Game that could disagree with it.
        std::optional<core::domain::Game> m_activeGame;
        std::optional<QString> m_pendingExecutablePath;

        bool m_running{false};
        bool m_databaseReady{false};
        std::chrono::seconds m_gameOpenDuration{0};
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
         * @brief Resolves a detected process to a game and requests a persisted session.
         */
        [[nodiscard]] bool
        startNewSession(const core::process::ProcessInfo &detectedProcess);

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
