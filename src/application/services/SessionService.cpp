#include "application/services/SessionService.h"
#include "application/services/GameService.h"
#include "logging/LoggingCategories.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include <QDateTime>

using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;

namespace gamelog::application::services {

SessionService::SessionService( SessionRepository &repository, const GameService &gameService)
    : repository_{repository},
    gameService_{gameService}
{
    restoreActiveSession();
}

vector<Session>
SessionService::search(const SessionQuery &query) const
{
    return repository_.query(query);
}

optional<Session>
SessionService::findActiveSession() const
{
    return activeSession_;
}

void SessionService::restoreActiveSession()
{
    SessionQuery query;
    query.statuses = {SessionStatus::Active};
    query.limit = 1;

    auto sessions = search(query);
    activeSession_ = sessions.empty() ? std::nullopt : optional{std::move(sessions.front())};
}

vector<Session>
SessionService::listSessionsForGame(int gameId) const
{
    SessionQuery query;
    query.gameIds = {gameId};
    return search(query);
}

optional<Session>
SessionService::startAutomaticSession(int gameId)
{
    auto requestedGame = gameService_.findById(gameId);
    if (activeSession_ || !requestedGame)
    {
        qCWarning(gamelogAgentLog)
            << "Cannot start an automatic session for game" << gameId;
        return std::nullopt;
    }

    Session session;
    session.gameId = gameId;
    session.startTimestamp = QDateTime::currentDateTimeUtc();
    session.trackedDuration = std::chrono::seconds::zero();
    session.source = SessionSource::Automatic;
    session.status = SessionStatus::Active;

    if (!repository_.insert(session))
    {
        return std::nullopt;
    }

    activeSession_ = session;
    emit sessionStarted(requestedGame.value());
    return activeSession_;
}

optional<Session> SessionService::endActiveSession()
{
    if (!activeSession_)
    {
        return std::nullopt;
    }

    Session completed = *activeSession_;
    const QDateTime endedAt = QDateTime::currentDateTimeUtc();
    completed.endTimestamp = endedAt;
    using DurationRep = std::chrono::seconds::rep;
    completed.trackedDuration = std::chrono::seconds{
        static_cast<DurationRep>(
            std::max<qint64>(0, completed.startTimestamp.secsTo(endedAt)))};
    completed.status = SessionStatus::Completed;

    if (!repository_.update(completed))
    {
        return std::nullopt;
    }

    activeSession_.reset();
    emit sessionStopped();
    return completed;
}

bool SessionService::addSession(Session &session)
{
    if (session.status == SessionStatus::Active && activeSession_)
    {
        return false;
    }

    if (!repository_.insert(session))
    {
        return false;
    }
    if (session.status == SessionStatus::Active)
    {
        activeSession_ = session;
    }
    return true;
}

bool SessionService::updateSession(const Session &session)
{
    if (!repository_.update(session))
    {
        return false;
    }

    if (session.status == SessionStatus::Active)
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

vector<Session> SessionService::getSessionsInDateRange(const QDate &startDate, const QDate &endDate) const
{
    return{};
}

} // namespace gamelog::application::services
