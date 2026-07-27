#include "sessions/SessionManager.h"

#include <QLoggingCategory>

#include "logging/LoggingCategories.h"

namespace gamelog::core::sessions
{
SessionManager::SessionManager(database::GameRepository &gameRepository, database::SessionRepository &sessionRepository)
    : m_gameRepository(gameRepository)
    , m_sessionRepository(sessionRepository)
{
}

std::optional<domain::Session> SessionManager::startAutomaticSession(int gameId)
{
    if (!m_gameRepository.findById(gameId).has_value())
    {
        qCWarning(gamelogCoreLog) << "Automatic session requested for unknown game ID:" << gameId;
        return std::nullopt;
    }

    qCInfo(gamelogCoreLog) << "Automatic session start is not implemented.";
    // TODO: Validate game and create active automatic session.
    return std::nullopt;
}

std::optional<domain::Session> SessionManager::startManualSession(int gameId)
{
    if (!m_gameRepository.findById(gameId).has_value())
    {
        qCWarning(gamelogCoreLog) << "Manual session requested for unknown game ID:" << gameId;
        return std::nullopt;
    }

    qCInfo(gamelogCoreLog) << "Manual session start is not implemented.";
    // TODO: Create active manual session and persist it.
    return std::nullopt;
}

bool SessionManager::endActiveSession()
{
    qCInfo(gamelogCoreLog) << "Ending active session is not implemented.";
    // TODO: Transition active session to completed/interrupted state.
    return false;
}

std::optional<domain::Session> SessionManager::activeSession()
{
    qCInfo(gamelogCoreLog) << "Active session lookup is a repository passthrough stub.";
    return m_sessionRepository.findActiveSession();
}
} // namespace gamelog::core::sessions
