#include "sessions/SessionManager.h"

#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "logging/LoggingCategories.h"

#include <algorithm>
#include <chrono>

#include <QDateTime>

namespace gamelog::core::sessions {

    SessionManager::SessionManager(
            database::GameRepository &gameRepository,
            database::SessionRepository &sessionRepository) :
        m_gameRepository{gameRepository},
        m_sessionRepository{sessionRepository}
    {}

    std::optional<domain::Session>
    SessionManager::startAutomaticSession(int gameId)
    {
        return startSession(gameId, domain::SessionSource::Automatic);
    }

    std::optional<domain::Session>
    SessionManager::startManualSession(int gameId)
    {
        return startSession(gameId, domain::SessionSource::Manual);
    }

    std::optional<domain::Session>
    SessionManager::startSession(int gameId, domain::SessionSource source)
    {
        if (m_activeSession)
        {
            qCWarning(gamelogCoreLog)
                    << "Attempted to start a session while another in-memory session is active.";
            return std::nullopt;
        }

        // The database constraint remains the final safety net, but checking first
        // produces a useful error and avoids relying on a UNIQUE failure for control
        // flow. It also detects an active row left by a previous agent process.
        if (const auto persistedSession = m_sessionRepository.findActiveSession();
            persistedSession)
        {
            qCWarning(gamelogCoreLog)
                    << "Cannot start a session because active session"
                    << persistedSession->id << "already exists in the database.";
            return std::nullopt;
        }

        const std::optional<domain::Game> game =
                m_gameRepository.findById(gameId);

        if (!game)
        {
            qCWarning(gamelogCoreLog)
                    << "Session requested for unknown game ID:" << gameId;
            return std::nullopt;
        }

        // Build a local candidate first. It is not considered active until the
        // repository insert succeeds and assigns its database ID.
        domain::Session candidate{
                .id = 0,
                .gameId = game->id,
                .startTimestamp = QDateTime::currentDateTimeUtc(),
                .endTimestamp = std::nullopt,
                .trackedDuration = std::chrono::seconds::zero(),
                .source = source,
                .status = domain::SessionStatus::Active,
        };

        if (!m_sessionRepository.insert(candidate))
        {
            qCWarning(gamelogCoreLog)
                    << "Failed to persist a new session for game ID:" << gameId;
            return std::nullopt;
        }

        // candidate now contains the generated primary key. Store exactly that
        // persisted object so the later UPDATE targets the correct row.
        m_activeSession = candidate;
        return candidate;
    }

    std::optional<domain::Session> SessionManager::endActiveSession()
    {
        if (!m_activeSession)
        {
            qCWarning(gamelogCoreLog)
                    << "Attempted to end a session when no in-memory session is active.";
            return std::nullopt;
        }

        // Work on a candidate copy. The current active value is left untouched
        // until the database update succeeds.
        domain::Session completed = *m_activeSession;
        const QDateTime endTimestamp = QDateTime::currentDateTimeUtc();
        const qint64 elapsedMilliseconds =
                completed.startTimestamp.msecsTo(endTimestamp);
        const qint64 nonNegativeMilliseconds =
                std::max<qint64>(elapsedMilliseconds, 0);

        completed.endTimestamp = endTimestamp;
        completed.trackedDuration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::milliseconds{
                        static_cast<std::chrono::milliseconds::rep>(
                                nonNegativeMilliseconds)});
        completed.status = domain::SessionStatus::Completed;

        if (!m_sessionRepository.update(completed))
        {
            qCWarning(gamelogCoreLog)
                    << "Failed to persist completion of session:" << completed.id;
            return std::nullopt;
        }

        m_activeSession.reset();
        return completed;
    }

    std::optional<domain::Session> SessionManager::activeSession() const
    {
        if (m_activeSession)
        {
            return m_activeSession;
        }

        return m_sessionRepository.findActiveSession();
    }

    bool SessionManager::hasActiveSession() const noexcept
    {
        return m_activeSession.has_value();
    }

} // namespace gamelog::core::sessions
