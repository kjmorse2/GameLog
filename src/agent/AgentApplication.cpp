#include "AgentApplication.h"

#include "logging/LoggingCategories.h"
#include "process/ProcfsProcessSource.h"
#include "domain/Game.h"
#include "database/GameRepository.h"

#include <memory>
#include <utility>

namespace gamelog::agent
{

AgentApplication::AgentApplication(
    QString databasePath
)
    : m_databaseManager{
          std::move(databasePath),
          "GameLogAgentConnection"
      }
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

    m_processSource =
        std::make_unique<core::process::ProcfsProcessSource>();

    const auto games = core::database::GameRepository(m_databaseManager.database()).findAll();

    for (const auto& game : games)
    {
        m_trackedExecutables.insert(game.executablePath.toStdString());
    }

    m_running = true;

    qCInfo(gamelogAgentLog) << "GameLog agent started";







    if(!syncGamesWithDatabase())
    {
        qCWarning(gamelogAgentLog) << "Failed to sync games with the database.";
    }

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

    qCInfo(gamelogAgentLog) << "Checking for games...";

    std::vector<core::process::ProcessInfo> processes = m_processSource->listProcesses();
    for (const auto& process : processes)
    {
        const std::string executablePath = process.executablePath.toStdString();

        if (m_trackedExecutables.find(executablePath) != m_trackedExecutables.end())
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
    if (!m_running)
    {
        qCWarning(gamelogAgentLog) << "Attempted to sync games with the database while the agent is not running.";

        return false;
    }

    const QSqlDatabase database = m_databaseManager.database();

    if (!database.isOpen())
    {
        qCWarning(gamelogAgentLog) << "Cannot sync games because the database is not open.";

        return false;
    }

    qCInfo(gamelogAgentLog) << "Syncing games with database...";

    return true;
}

} // namespace gamelog::agent