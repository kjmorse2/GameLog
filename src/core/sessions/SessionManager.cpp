#include "sessions/SessionManager.h"

#include "domain/Session.h"

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
    if(m_isSessionActive)
    {
        qCWarning(gamelogCoreLog) << "Attempted to start a new automatic session while another session is active.";
        return std::nullopt;
    }

    std::optional<domain::Game> potentialGame = m_gameRepository.findById(gameId);
    if (!potentialGame.has_value())
    {
        qCWarning(gamelogCoreLog) << "Automatic session requested for unknown game ID:" << gameId;
        return std::nullopt;
    }

    m_activeGame = potentialGame.value();
    m_activeSession.id = 0; 
    m_activeSession.gameId = m_activeGame.id;
    m_activeSession.startTimestamp = QDateTime::currentDateTime();
    m_activeSession.endTimestamp = QDateTime();
    m_activeSession.trackedDuration = std::chrono::seconds(0);
    m_activeSession.source = domain::SessionSource::Automatic;
    m_activeSession.status = domain::SessionStatus::Active;
    m_isSessionActive = true;
    return m_activeSession;
}

std::optional<domain::Session> SessionManager::startManualSession(int gameId)
{
    if(m_isSessionActive)
    {
        qCWarning(gamelogCoreLog) << "Attempted to start a new manual session while another session is active.";
        return std::nullopt;
    }

    std::optional<domain::Game> potentialGame = m_gameRepository.findById(gameId);
    if (!potentialGame.has_value())
    {
        qCWarning(gamelogCoreLog) << "Manual session requested for unknown game ID:" << gameId;
        return std::nullopt;
    }

    m_activeGame = potentialGame.value();
    m_activeSession.id = 0; 
    m_activeSession.gameId = m_activeGame.id;
    m_activeSession.startTimestamp = QDateTime::currentDateTime();
    m_activeSession.endTimestamp = QDateTime();
    m_activeSession.trackedDuration = std::chrono::seconds(0);
    m_activeSession.source = domain::SessionSource::Manual;
    m_activeSession.status = domain::SessionStatus::Active;
    m_isSessionActive = true;
    return m_activeSession;
}

bool SessionManager::endActiveSession()
{
    if(!m_isSessionActive)
    {
        qCWarning(gamelogCoreLog) << "Attempted to end a session when no session is active.";
        return false;
    }
    m_activeSession.endTimestamp = QDateTime::currentDateTime();
    m_activeSession.status = domain::SessionStatus::Completed;
    m_isSessionActive = false;
    return true;
}

std::optional<domain::Session> SessionManager::activeSession()
{
    qCInfo(gamelogCoreLog) << "Active session lookup is a repository passthrough stub.";
    return m_sessionRepository.findActiveSession();
}
} // namespace gamelog::core::sessions
