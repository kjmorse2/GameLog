#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "database/DatabaseManager.h"
#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "process/ProcessSource.h"
#include "sessions/SessionManager.h"

namespace gamelog::agent {

    /**
     * @brief Owns the background agent runtime and its cached detection state.
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
         */
        void start();

        /**
         * @brief Stops process tracking and releases the process source.
         */
        void stop();

        /**
         * @brief Scans the current process table for tracked executables.
         */
        void updateAgent(int);


    private:
        bool m_running{false};
        bool m_recording{false};
        bool m_databaseReady{false};
        int m_secondsGameHasBeenOpened{0};
        int m_secondsGameHasBeenClosed{0};

        std::unique_ptr<core::process::ProcessSource> m_processSource;
        std::unordered_set<std::string> m_trackedExecutables;
        core::database::DatabaseManager m_databaseManager;
        std::optional<core::database::SessionRepository> m_sessionRepository;
        std::optional<core::database::GameRepository> m_gameRepository;
        std::optional<core::sessions::SessionManager> m_sessionManager;
        core::domain::Session m_activeSession;
        core::domain::Game m_activeGame;

        /**
         * @brief Rebuilds the tracked executable set from the current database.
         */
        [[nodiscard]] bool syncGamesWithDatabase();

        void startNewSession(const core::domain::Game &);
        void stopActiveSession();
    };

} // namespace gamelog::agent
