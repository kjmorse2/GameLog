#include "AgentApplication.h"

#include "database/GameRepository.h"
#include "logging/LoggingCategories.h"
#include "process/ProcfsProcessSource.h"

#include <memory>
#include <utility>
#include <vector>

#include <QSqlDatabase>

namespace gamelog::agent {

    AgentApplication::AgentApplication(QString databasePath) : m_databaseManager{std::move(databasePath), "GameLogAgentConnection"}
    {
        m_databaseReady = m_databaseManager.initialize();

        if (!m_databaseReady)
        {
            qCWarning(gamelogAgentLog) << "Failed to initialize the database manager.";
        }
    }

    void AgentApplication::start()
    {
        if (m_running)
        {
            qCWarning(gamelogAgentLog) << "Attempted to start an already-running agent.";
            return;
        }

        if (!m_databaseReady)
        {
            qCWarning(gamelogAgentLog) << "Cannot start the agent because the database was not initialized.";
            return;
        }

        // The process source is lightweight, so create it only when the agent starts.
        m_processSource = std::make_unique<core::process::ProcfsProcessSource>();

        if (!syncGamesWithDatabase())
        {
            qCWarning(gamelogAgentLog) << "Failed to sync games with the database.";
        }

        m_running = true;

        qCInfo(gamelogAgentLog) << "GameLog agent started";
        qCInfo(gamelogAgentLog) << "Database is: " << (m_databaseManager.isOpen() ? "open" : "closed");
        qCInfo(gamelogAgentLog) << "Database path: " << m_databaseManager.database().databaseName();
    }

    void AgentApplication::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;
        m_processSource.reset();

        qCInfo(gamelogAgentLog) << "GameLog agent stopped";
    }

    void AgentApplication::checkForGames()
    {
        if (!m_running)
        {
            qCWarning(gamelogAgentLog) << "Attempted to check for games while the agent is not running.";
            return;
        }

        if (!m_processSource)
        {
            qCWarning(gamelogAgentLog) << "Cannot check processes because the process source is unavailable.";
            return;
        }

        // Poll the current process table and look for any executable path we cached.
        qCInfo(gamelogAgentLog) << "Checking for games...";

        std::vector<core::process::ProcessInfo> processes = m_processSource->listProcesses();
        for (const auto &process: processes)
        {
            const std::string executablePath = process.executablePath.toStdString();

            if (m_trackedExecutables.contains(executablePath))
            {
                qCInfo(gamelogAgentLog) << "Found tracked game process:"
                                        << "PID:" << process.pid
                                        << "Executable Name:" << process.executableName
                                        << "Executable Path:" << process.executablePath;
            }
        }
    }

    bool AgentApplication::syncGamesWithDatabase()
    {
        const QSqlDatabase database = m_databaseManager.database();

        if (!database.isOpen())
        {
            qCWarning(gamelogAgentLog) << "Cannot sync games because the database is not open.";

            return false;
        }

        // Rebuild the cache from scratch so stale executable paths do not linger.
        m_trackedExecutables.clear();

        const auto games = core::database::GameRepository(database).findAll();

        for (const auto &game: games)
        {
            if (!game.executablePath.isEmpty())
            {
                m_trackedExecutables.insert(game.executablePath.toStdString());
            }
        }

        qCInfo(gamelogAgentLog) << "Syncing games with database...";

        return true;
    }

} // namespace gamelog::agent
