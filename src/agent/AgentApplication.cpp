#include "AgentApplication.h"
#include <process/ProcfsProcessSource.h>
#include <process/ProcessInfo.h>

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::agent
{

AgentApplication::AgentApplication(std::string databasePath)
    : m_running{false},
      m_trackedGames{}
{
    // Initialize the database manager with the provided path
    gamelog::core::database::DatabaseManager m_databaseManager{QString::fromStdString(databasePath), "GameLogAgentConnection"};
    if (!m_databaseManager.initialize())
    {
        qCWarning(gamelogAgentLog) << "Failed to initialize the database manager.";
    }
    m_database = m_databaseManager.database();
}

void AgentApplication::start()
{
    m_running = true;
    qCInfo(gamelogAgentLog) << "GameLog agent started";
    m_processSource = new core::process::ProcfsProcessSource();
    m_trackedGames = std::unordered_set<std::string>{};
    syncGamesWithDatabase();
}

void AgentApplication::stop()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;
    qCInfo(gamelogAgentLog) << "GameLog agent stopping";
}

void AgentApplication::checkForGames()
{
    if (!m_running)
    {
        qCWarning(gamelogAgentLog) << "Attempted to check for games while the agent is not running.";
        return;
    }

    qCInfo(gamelogAgentLog) << "Checking for games...";
    std::vector<core::process::ProcessInfo> processes = m_processSource->listProcesses();
    for (const auto& process : processes)
    {
        qCInfo(gamelogAgentLog) << "Found process:" << process.pid << process.executableName << process.executablePath;
    }
} 

bool AgentApplication::syncGamesWithDatabase()
{
    if (!m_running)
    {
        qCWarning(gamelogAgentLog) << "Attempted to sync games with database while the agent is not running.";
        return false;
    }

    qCInfo(gamelogAgentLog) << "Syncing games with database...";

    return true;
}
}// namespace gamelog::agent