#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "database/DatabaseManager.h"
#include "process/ProcessSource.h"

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
        void checkForGames();

        /**
         * @brief Rebuilds the tracked executable set from the current database.
         */
        [[nodiscard]] bool syncGamesWithDatabase();

    private:
        bool m_running{false};
        bool m_databaseReady{false};

        std::unique_ptr<core::process::ProcessSource> m_processSource;

        std::unordered_set<std::string> m_trackedExecutables;

        core::database::DatabaseManager m_databaseManager;
    };

} // namespace gamelog::agent
