#include "application/services/SessionService.h"

#include "application/services/GameService.h"
#include "logging/LoggingCategories.h"
#include "process/ProcessHelpers.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include <QDateTime>

using gamelog::core::domain::Game;
using gamelog::core::domain::Session;
using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::process::ProcessHelpers;
using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace gamelog::application::services
{
    SessionService::SessionService(core::database::SessionRepository& repository, const GameService& gameService): repository_{repository},
                                                                                                                   gameService_{gameService}
    {
    }

    std::vector<Session> SessionService::search(const SessionQuery& query) const
    {
        return repository_.query(query);
    }

    std::optional<Session> SessionService::findActiveSession() const
    {
        if(activeSession_)
        {
            return *activeSession_;
        }

        qCWarning(gamelogRuntimeLog) << "Active session was not found";
        return std::nullopt;
    }

    std::vector<Session> SessionService::listSessionsForGame(int gameId) const
    {
        SessionQuery query;
        query.gameIds = {gameId};
        return search(query);
    }

    std::vector<Session> SessionService::getSessionsInTimeRange(const QDateTime& startDate, const QDateTime& endDate) const
    {
        SessionQuery query;
        query.startedAtOrAfter = startDate;
        query.startedBefore = endDate;
        return search(query);
    }

    std::optional<Session> SessionService::startAutomaticSession(int gameId)
    {
        auto requestedGame = gameService_.findById(gameId);
        if(!requestedGame)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start an automatic session for missing game" << gameId;
            return std::nullopt;
        }

        return startAutomaticSession(*requestedGame);
    }

    std::optional<Session> SessionService::endActiveSession()
    {
        if(!activeSession_)
        {
            return std::nullopt;
        }

        Session completed = *activeSession_;
        const QDateTime endedAt = QDateTime::currentDateTimeUtc();
        completed.endTimestamp = endedAt;

        using DurationRep = seconds::rep;
        completed.trackedDuration = seconds{static_cast<DurationRep>(std::max<qint64>(0, completed.startTimestamp.secsTo(endedAt)))};
        completed.status = SessionStatus::Completed;

        if(!repository_.update(completed))
        {
            return std::nullopt;
        }

        const QString gameTitle = activeGame_ ? activeGame_->title : QString{};
        activeSession_.reset();
        activeGame_.reset();
        resetPendingStart();
        gameClosedDuration_ = seconds::zero();

        qCInfo(gamelogRuntimeLog) << "Stopped session" << completed.id << "for game:" << gameTitle;

        emit sessionStopped(completed);
        return completed;
    }

    bool SessionService::addSession(Session& session)
    {
        if(session.status == SessionStatus::Active && activeSession_)
        {
            return false;
        }

        std::optional<Game> sessionGame;
        if(session.status == SessionStatus::Active)
        {
            sessionGame = gameService_.findById(session.gameId);
            if(!sessionGame)
            {
                return false;
            }
        }

        if(!repository_.insert(session))
        {
            return false;
        }

        if(session.status == SessionStatus::Active)
        {
            activeSession_ = session;
            activeGame_ = std::move(sessionGame);
        }
        return true;
    }

    bool SessionService::updateSession(const Session& session)
    {
        std::optional<Game> sessionGame;
        if(session.status == SessionStatus::Active)
        {
            sessionGame = gameService_.findById(session.gameId);
            if(!sessionGame)
            {
                return false;
            }
        }

        if(!repository_.update(session))
        {
            return false;
        }

        if(session.status == SessionStatus::Active)
        {
            activeSession_ = session;
            activeGame_ = std::move(sessionGame);
        }
        else if(activeSession_&& activeSession_->id == session.id)
        {
            activeSession_.reset();
            activeGame_.reset();
            resetAutomaticTracking();
        }
        return true;
    }

    bool SessionService::removeSession(int sessionId)
    {
        if(!repository_.remove(sessionId))
        {
            return false;
        }

        if(activeSession_&& activeSession_->id == sessionId)
        {
            activeSession_.reset();
            activeGame_.reset();
            resetAutomaticTracking();
        }
        return true;
    }

    bool SessionService::restoreActiveSession()
    {
        SessionQuery query;
        query.statuses = {SessionStatus::Active};
        query.limit = 1;

        auto sessions = search(query);
        if(sessions.empty())
        {
            activeSession_.reset();
            activeGame_.reset();
            resetAutomaticTracking();
            return true;
        }

        Session restoredSession = std::move(sessions.front());
        const auto restoredGame = gameService_.findById(restoredSession.gameId);
        if(!restoredGame)
        {
            qCWarning(gamelogRuntimeLog) << "Active session" << restoredSession.id << "references missing game" << restoredSession.gameId;
            return false;
        }

        activeSession_ = std::move(restoredSession);
        activeGame_ = std::move(*restoredGame);
        resetPendingStart();
        gameClosedDuration_ = seconds::zero();

        qCInfo(gamelogRuntimeLog) << "Restored active session" << activeSession_->id << "for game:" << activeGame_->title;
        return true;
    }

    void SessionService::updateAutomaticTracking(const std::vector<ProcessInfo>& processes, seconds elapsed)
    {
        if(elapsed <= seconds::zero())
        {
            return;
        }

        if(!activeSession_)
        {
            const Game* detectedGame = nullptr;
            for(const ProcessInfo& process : processes)
            {
                detectedGame = ProcessHelpers::matchTrackedGame(process, gameService_.trackedSteamGames(), gameService_.trackedPathGames());

                if(detectedGame)
                {
                    break;
                }
            }

            if(!detectedGame)
            {
                resetPendingStart();
                return;
            }

            if(!pendingGameId_ || *pendingGameId_ != detectedGame->id)
            {
                pendingGameId_ = detectedGame->id;
                gameOpenDuration_ = seconds::zero();
            }

            gameOpenDuration_ += elapsed;
            if(gameOpenDuration_ < kStartGracePeriod)
            {
                return;
            }

            static_cast<void>(startAutomaticSession(*detectedGame));
            resetPendingStart();
            return;
        }

        // restoreActiveSession() and startAutomaticSession() normally maintain
        // this invariant. Rehydrate defensively if an active Session was supplied
        // through another service operation.
        if(!activeGame_)
        {
            activeGame_ = gameService_.findById(activeSession_->gameId);
            if(!activeGame_)
            {
                qCWarning(gamelogRuntimeLog) << "Cannot track active session" << activeSession_->id << "because game" << activeSession_->gameId << "is unavailable.";
                return;
            }
        }

        const bool activeGameFound = std::ranges::any_of(
                processes,
                [this](const ProcessInfo& process)
                {
                    return ProcessHelpers::processMatchesGame(process, *activeGame_);
                }
                );

        if(activeGameFound)
        {
            gameClosedDuration_ = seconds::zero();
            return;
        }

        gameClosedDuration_ += elapsed;
        if(gameClosedDuration_ >= kEndGracePeriod)
        {
            static_cast<void>(endActiveSession());
        }
    }

    void SessionService::resetAutomaticTracking() noexcept
    {
        resetPendingStart();
        gameClosedDuration_ = seconds::zero();
        activeGame_.reset();
    }

    std::optional<Session> SessionService::startAutomaticSession(const Game& game)
    {
        if(activeSession_)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start an automatic session while another session is active.";
            return std::nullopt;
        }

        Session session;
        session.gameId = game.id;
        session.startTimestamp = QDateTime::currentDateTimeUtc();
        session.trackedDuration = seconds::zero();
        session.source = SessionSource::Automatic;
        session.status = SessionStatus::Active;

        if(!repository_.insert(session))
        {
            return std::nullopt;
        }

        activeSession_ = session;
        activeGame_ = game;
        gameClosedDuration_ = seconds::zero();

        qCInfo(gamelogRuntimeLog) << "Started session" << activeSession_->id << "for game:" << activeGame_->title;

        emit sessionStarted(*activeGame_);
        return activeSession_;
    }

    void SessionService::resetPendingStart() noexcept
    {
        pendingGameId_.reset();
        gameOpenDuration_ = seconds::zero();
    }
} // namespace gamelog::application::services
