#include "AgentApplication.h"
#include <process/ProcfsProcessSource.h>
#include <process/ProcessInfo.h>

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::agent
{
void AgentApplication::start()
{
    m_running = true;
    qCInfo(gamelogAgentLog) << "GameLog agent started";
    m_processSource = new core::process::ProcfsProcessSource();
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
}// namespace gamelog::agent