#include "application/services/SessionService.h"

#include "application/services/GameService.h"
#include "logging/LoggingCategories.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include <QDateTime>

namespace gamelog::application::services {

SessionService::SessionService(
    core::database::SessionRepository &repository,
    const GameService &gameService)
    : repository_{repository}, gameService_{gameService}
{
    restoreActiveSession();
}

std::vector<core::domain::Session>
SessionService::search(const core::domain::query::SessionQuery &query) const
{
    return repository_.query(query);
}

std::optional<core::domain::Session>
SessionService::findActiveSession() const
{
    return activeSession_;
}

void SessionService::restoreActiveSession()
{
    core::domain::query::SessionQuery query;
    query.statuses = {core::domain::SessionStatus::Active};
    query.limit = 1;

    auto sessions = search(query);
    activeSession_ = sessions.empty()
        ? std::nullopt
        : std::optional<core::domain::Session>{std::move(sessions.front())};
}

std::vector<core::domain::Session>
SessionService::listSessionsForGame(int gameId) const
{
    core::domain::query::SessionQuery query;
    query.gameIds = {gameId};
    return search(query);
}

std::optional<core::domain::Session>
SessionService::startAutomaticSession(int gameId)
{
    if (activeSession_ || !gameService_.findById(gameId))
    {
        qCWarning(gamelogAgentLog)
            << "Cannot start an automatic session for game" << gameId;
        return std::nullopt;
    }

    core::domain::Session session;
    session.gameId = gameId;
    session.startTimestamp = QDateTime::currentDateTimeUtc();
    session.trackedDuration = std::chrono::seconds::zero();
    session.source = core::domain::SessionSource::Automatic;
    session.status = core::domain::SessionStatus::Active;

    if (!repository_.insert(session))
    {
        return std::nullopt;
    }

    activeSession_ = session;
    return activeSession_;
}

std::optional<core::domain::Session> SessionService::endActiveSession()
{
    if (!activeSession_)
    {
        return std::nullopt;
    }

    core::domain::Session completed = *activeSession_;
    const QDateTime endedAt = QDateTime::currentDateTimeUtc();
    completed.endTimestamp = endedAt;
    using DurationRep = std::chrono::seconds::rep;
    completed.trackedDuration = std::chrono::seconds{
        static_cast<DurationRep>(
            std::max<qint64>(0, completed.startTimestamp.secsTo(endedAt)))};
    completed.status = core::domain::SessionStatus::Completed;

    if (!repository_.update(completed))
    {
        return std::nullopt;
    }

    activeSession_.reset();
    return completed;
}

bool SessionService::addSession(core::domain::Session &session)
{
    if (session.status == core::domain::SessionStatus::Active && activeSession_)
    {
        return false;
    }

    if (!repository_.insert(session))
    {
        return false;
    }
    if (session.status == core::domain::SessionStatus::Active)
    {
        activeSession_ = session;
    }
    return true;
}

bool SessionService::updateSession(const core::domain::Session &session)
{
    if (!repository_.update(session))
    {
        return false;
    }

    if (session.status == core::domain::SessionStatus::Active)
    {
        activeSession_ = session;
    }
    else if (activeSession_ && activeSession_->id == session.id)
    {
        activeSession_.reset();
    }
    return true;
}

bool SessionService::removeSession(int sessionId)
{
    if (!repository_.remove(sessionId))
    {
        return false;
    }
    if (activeSession_ && activeSession_->id == sessionId)
    {
        activeSession_.reset();
    }
    return true;
}

} // namespace gamelog::application::services
