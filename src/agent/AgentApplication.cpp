#include "AgentApplication.h"

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::agent
{
void AgentApplication::start()
{
    m_running = true;
    qCInfo(gamelogAgentLog) << "GameLog agent started";
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
} // namespace gamelog::agent
