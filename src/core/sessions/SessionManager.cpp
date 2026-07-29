#include "sessions/SessionManager.h"

#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "logging/LoggingCategories.h"

#include <chrono>

#include <QDateTime>

namespace gamelog::core::sessions {
    SessionManager::SessionManager(database::GameRepository &gameRepository, database::SessionRepository &sessionRepository) :
        m_gameRepository(gameRepository),
        m_sessionRepository(sessionRepository)
    {}

    std::optional<domain::Session> SessionManager::startAutomaticSession(int gameId)
    {
        // Guard the one-active-session rule before touching any repository state.
        if (m_isSessionActive)
        {
            qCWarning(gamelogCoreLog) << "Attempted to start a new automatic session while another session is active.";
            return std::nullopt;
        }

        // Resolve the game once so the manager can cache both the game and session.
        const std::optional<domain::Game> potentialGame = m_gameRepository.findById(gameId);
        if (!potentialGame.has_value())
        {
            qCWarning(gamelogCoreLog) << "Automatic session requested for unknown game ID:" << gameId;
            return std::nullopt;
        }

        m_activeGame = potentialGame.value();
        m_activeSession.id = 0;
        m_activeSession.gameId = m_activeGame.id;
        m_activeSession.startTimestamp = QDateTime::currentDateTime();
        m_activeSession.endTimestamp.reset();
        m_activeSession.trackedDuration = std::chrono::seconds{0};
        m_activeSession.source = domain::SessionSource::Automatic;
        m_activeSession.status = domain::SessionStatus::Active;
        m_isSessionActive = true;
        return m_activeSession;
    }

    std::optional<domain::Session> SessionManager::startManualSession(int gameId)
    {
        // Manual starts obey the same exclusivity rule as automatic starts.
        if (m_isSessionActive)
        {
            qCWarning(gamelogCoreLog) << "Attempted to start a new manual session while another session is active.";
            return std::nullopt;
        }

        const std::optional<domain::Game> potentialGame = m_gameRepository.findById(gameId);
        if (!potentialGame.has_value())
        {
            qCWarning(gamelogCoreLog) << "Manual session requested for unknown game ID:" << gameId;
            return std::nullopt;
        }

        m_activeGame = potentialGame.value();
        m_activeSession.id = 0;
        m_activeSession.gameId = m_activeGame.id;
        m_activeSession.startTimestamp = QDateTime::currentDateTime();
        m_activeSession.endTimestamp.reset();
        m_activeSession.trackedDuration = std::chrono::seconds{0};
        m_activeSession.source = domain::SessionSource::Manual;
        m_activeSession.status = domain::SessionStatus::Active;
        m_isSessionActive = true;
        return m_activeSession;
    }

    std::optional<domain::Session> SessionManager::endActiveSession()
    {
        // If nothing is active, there is nothing to persist or close out.
        if (!m_isSessionActive)
        {
            qCWarning(gamelogCoreLog) << "Attempted to end a session when no session is active.";
            return std::nullopt;
        }

        m_activeSession.endTimestamp = QDateTime::currentDateTime();
        m_activeSession.status = domain::SessionStatus::Completed;
        m_isSessionActive = false;

        // The repository write-back can be added once the lifecycle is fully wired.
        return m_activeSession;
    }

    std::optional<domain::Session> SessionManager::activeSession()
    {
        // Prefer the in-memory session while the manager is actively tracking one.
        if (m_isSessionActive)
        {
            return m_activeSession;
        }

        // Fall back to the repository when the manager is idle or recovering state.
        return m_sessionRepository.findActiveSession();
    }
} // namespace gamelog::core::sessions
