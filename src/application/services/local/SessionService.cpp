#include "application/services/local/SessionService.h"

#include "application/services/local/GameService.h"
#include "logging/LoggingCategories.h"
#include "process/ProcessHelpers.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

using gamelog::core::domain::Game;
using gamelog::core::domain::Session;
using gamelog::core::domain::SessionSource;
using gamelog::core::domain::SessionStatus;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::domain::query::SessionSortField;
using gamelog::core::domain::query::SortDirection;
using gamelog::core::process::ProcessHelpers;
using gamelog::core::process::ProcessInfo;
using std::chrono::seconds;

namespace gamelog::application::services
{
    SessionService::SessionService(core::database::SessionRepository& repository, const GameService& gameService)
        : SessionService{repository, gameService, [] { return QDateTime::currentDateTimeUtc(); }} {}

    SessionService::SessionService(core::database::SessionRepository& repository,
                                   const GameService& gameService,
                                   Clock clock)
        : repository_{repository},
          gameService_{gameService},
          clock_{std::move(clock)} {}

    std::vector<Session> SessionService::search(const SessionQuery& query) const { return repository_.query(query); }

    std::optional<Session> SessionService::findActiveSession() const
    {
        if(activeSession_) { return *activeSession_; }

        qCWarning(gamelogRuntimeLog) << "Active session was not found";
        return std::nullopt;
    }

    std::vector<Session> SessionService::listSessionsForGame(int gameId) const
    {
        SessionQuery query;
        query.gameIds = {gameId};
        return search(query);
    }

    std::vector<Session> SessionService::getSessionsInTimeRange(const QDateTime& startDate,
                                                                const QDateTime& endDate) const
    {
        SessionQuery query;
        query.startedAtOrAfter = startDate;
        query.startedBefore = endDate;
        return search(query);
    }

    std::optional<Session> SessionService::startAutomaticSession(int gameId)
    {
        const auto requestedGame = gameService_.findById(gameId);
        if(!requestedGame)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start an automatic session for missing game" << gameId;
            return std::nullopt;
        }

        if(!requestedGame->trackingEnabled)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start an automatic session for untracked game" << gameId;
            return std::nullopt;
        }

        return startAutomaticSession(*requestedGame);
    }

    std::optional<Session> SessionService::endActiveSession()
    {
        if(!activeSession_) { return std::nullopt; }

        const QDateTime endedAt = currentDateTimeUtc();
        if(!endedAt.isValid() || !activeSession_->startTimestamp.isValid() || endedAt < activeSession_->startTimestamp)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot end session" << activeSession_->id <<
                "because its timestamps would be invalid.";
            return std::nullopt;
        }

        Session completed = *activeSession_;
        completed.endTimestamp = endedAt;
        completed.trackedDuration = seconds{completed.startTimestamp.secsTo(endedAt)};
        completed.status = SessionStatus::Completed;

        if(!repository_.update(completed)) { return std::nullopt; }

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
        std::optional<Game> sessionGame;
        if(session.status == SessionStatus::Active)
        {
            if(activeSession_ || hasOtherActiveSession())
            {
                qCWarning(gamelogRuntimeLog) << "Cannot add a second active session.";
                return false;
            }

            sessionGame = gameService_.findById(session.gameId);
            if(!sessionGame) { return false; }
        }

        if(!repository_.insert(session)) { return false; }

        if(session.status == SessionStatus::Active)
        {
            activeSession_ = session;
            activeGame_ = std::move(sessionGame);
            resetPendingStart();
            gameClosedDuration_ = seconds::zero();
            emit sessionStarted(*activeGame_);
        }

        return true;
    }

    bool SessionService::updateSession(const Session& session)
    {
        SessionQuery existingQuery;
        existingQuery.ids = {session.id};
        existingQuery.limit = 1;

        const std::vector<Session> existingSessions = search(existingQuery);
        if(existingSessions.empty()) { return false; }

        const Session& existing = existingSessions.front();
        const bool wasActive = existing.status == SessionStatus::Active;
        const bool willBeActive = session.status == SessionStatus::Active;

        std::optional<Game> sessionGame;
        if(willBeActive)
        {
            if((activeSession_ && activeSession_->id != session.id) || hasOtherActiveSession(session.id))
            {
                qCWarning(gamelogRuntimeLog) << "Cannot update a second session to active.";
                return false;
            }

            sessionGame = gameService_.findById(session.gameId);
            if(!sessionGame) { return false; }
        }

        if(!repository_.update(session)) { return false; }

        if(!wasActive && willBeActive)
        {
            activeSession_ = session;
            activeGame_ = std::move(sessionGame);
            resetPendingStart();
            gameClosedDuration_ = seconds::zero();
            emit sessionStarted(*activeGame_);
        }
        else if(wasActive && !willBeActive)
        {
            if(activeSession_&& activeSession_->id == session.id)
            {
                activeSession_.reset();
                activeGame_.reset();
                resetPendingStart();
                gameClosedDuration_ = seconds::zero();
            }

            emit sessionStopped(session);
        }
        else if(willBeActive)
        {
            activeSession_ = session;
            activeGame_ = std::move(sessionGame);
        }

        return true;
    }

    bool SessionService::removeSession(int sessionId)
    {
        SessionQuery query;
        query.ids = {sessionId};
        query.limit = 1;

        const std::vector<Session> sessions = search(query);
        if(sessions.empty() || sessions.front().status == SessionStatus::Active) { return false; }

        return repository_.remove(sessionId);
    }

    bool SessionService::restoreActiveSession()
    {
        activeSession_.reset();
        activeGame_.reset();
        resetPendingStart();
        gameClosedDuration_ = seconds::zero();

        SessionQuery query;
        query.statuses = {SessionStatus::Active};
        query.sortBy = SessionSortField::StartTimestamp;
        query.sortDirection = SortDirection::Descending;

        std::vector<Session> sessions = search(query);
        if(sessions.empty()) { return true; }

        std::sort(sessions.begin(),
                  sessions.end(),
                  [](const Session& left, const Session& right)
                  {
                      if(left.startTimestamp != right.startTimestamp)
                      {
                          return left.startTimestamp > right.startTimestamp;
                      }

                      return left.id > right.id;
                  });

        const QDateTime restoredAt = currentDateTimeUtc();
        if(!restoredAt.isValid()) { return false; }

        for(Session& session : sessions)
        {
            const auto game = gameService_.findById(session.gameId);

            if(!activeSession_ && game)
            {
                activeSession_ = session;
                activeGame_ = *game;
                continue;
            }

            if(!game)
            {
                qCWarning(gamelogRuntimeLog) << "Active session" << session.id << "references missing game" << session.
                    gameId << "and will be interrupted.";
            }
            else
            {
                qCWarning(gamelogRuntimeLog) << "Extra active session" << session.id <<
                    "will be interrupted while restoring" << activeSession_->id;
            }

            if(!interruptSession(session, restoredAt))
            {
                activeSession_.reset();
                activeGame_.reset();
                return false;
            }
        }

        if(activeSession_)
        {
            qCInfo(gamelogRuntimeLog) << "Restored active session" << activeSession_->id << "for game:" << activeGame_->
                title;
        }

        return true;
    }

    void SessionService::updateAutomaticTracking(const std::vector<ProcessInfo>& processes, seconds elapsed)
    {
        if(elapsed <= seconds::zero()) { return; }

        if(!activeSession_)
        {
            const std::optional<Game> detectedGame = selectDetectedGame(processes);
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
            if(gameOpenDuration_ < kStartGracePeriod) { return; }

            static_cast<void>(startAutomaticSession(*detectedGame));
            resetPendingStart();
            return;
        }

        // restoreActiveSession() and startAutomaticSession() normally maintain
        // this invariant. Rehydrate defensively if an active Session was supplied
        // through another service operation or automatic tracking was reset.
        if(!activeGame_)
        {
            activeGame_ = gameService_.findById(activeSession_->gameId);
            if(!activeGame_)
            {
                qCWarning(gamelogRuntimeLog) << "Cannot track active session" << activeSession_->id << "because game" <<
                    activeSession_->gameId << "is unavailable.";
                return;
            }
        }

        const bool activeGameFound = std::ranges::any_of(processes,
                                                         [this](const ProcessInfo& process)
                                                         {
                                                             return ProcessHelpers::processMatchesGame(process,
                                                                 *activeGame_);
                                                         });

        if(activeGameFound)
        {
            gameClosedDuration_ = seconds::zero();
            return;
        }

        gameClosedDuration_ += elapsed;
        if(gameClosedDuration_ >= kEndGracePeriod) { static_cast<void>(endActiveSession()); }
    }

    void SessionService::resetAutomaticTracking() noexcept
    {
        resetPendingStart();
        gameClosedDuration_ = seconds::zero();
        activeGame_.reset();
    }

    std::optional<Session> SessionService::startAutomaticSession(const Game& game)
    {
        if(activeSession_ || hasOtherActiveSession())
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start an automatic session while another session is active.";
            return std::nullopt;
        }

        if(game.id <= 0 || !game.trackingEnabled)
        {
            qCWarning(gamelogRuntimeLog) << "Cannot start an automatic session for an invalid or untracked game.";
            return std::nullopt;
        }

        const QDateTime startedAt = currentDateTimeUtc();
        if(!startedAt.isValid()) { return std::nullopt; }

        Session session;
        session.gameId = game.id;
        session.startTimestamp = startedAt;
        session.trackedDuration = seconds::zero();
        session.source = SessionSource::Automatic;
        session.status = SessionStatus::Active;

        if(!repository_.insert(session)) { return std::nullopt; }

        activeSession_ = session;
        activeGame_ = game;
        gameClosedDuration_ = seconds::zero();

        qCInfo(gamelogRuntimeLog) << "Started session" << activeSession_->id << "for game:" << activeGame_->title;

        emit sessionStarted(*activeGame_);
        return activeSession_;
    }

    QDateTime SessionService::currentDateTimeUtc() const
    {
        if(!clock_) { return {}; }

        const QDateTime value = clock_();
        return value.isValid() ? value.toUTC() : QDateTime{};
    }

    bool SessionService::hasOtherActiveSession(int excludedSessionId) const
    {
        SessionQuery query;
        query.statuses = {SessionStatus::Active};

        for(const Session& active : search(query)) { if(active.id != excludedSessionId) { return true; } }

        return false;
    }

    bool SessionService::interruptSession(Session session, const QDateTime& interruptedAt)
    {
        if(!session.startTimestamp.isValid() || !interruptedAt.isValid()) { return false; }

        const QDateTime endedAt = interruptedAt < session.startTimestamp ? session.startTimestamp : interruptedAt;
        session.endTimestamp = endedAt;
        session.trackedDuration = seconds{session.startTimestamp.secsTo(endedAt)};
        session.status = SessionStatus::Interrupted;

        if(!repository_.update(session)) { return false; }

        emit sessionStopped(session);
        return true;
    }

    std::optional<Game> SessionService::selectDetectedGame(const std::vector<ProcessInfo>& processes) const
    {
        struct Candidate
        {
            Game game;
            int priority;
        };

        std::vector<Candidate> candidates;

        const auto addCandidate = [&candidates](const Game& game, int priority)
        {
            const auto existing = std::find_if(candidates.begin(),
                                               candidates.end(),
                                               [&game](const Candidate& candidate)
                                               {
                                                   return candidate.game.id == game.id;
                                               });

            if(existing == candidates.end()) { candidates.push_back({game, priority}); }
            else { existing->priority = std::min(existing->priority, priority); }
        };

        for(const ProcessInfo& process : processes)
        {
            if(process.steamAppId)
            {
                const auto steamGame = gameService_.trackedSteamGames().constFind(*process.steamAppId);
                if(steamGame != gameService_.trackedSteamGames().constEnd())
                {
                    addCandidate(steamGame.value(), 0);
                    continue;
                }
            }

            if(!process.executablePath.isEmpty())
            {
                const auto pathGame = gameService_.trackedPathGames().constFind(process.executablePath);
                if(pathGame != gameService_.trackedPathGames().constEnd() &&
                   ProcessHelpers::processMatchesGame(process, pathGame.value())) { addCandidate(pathGame.value(), 1); }
            }
        }

        if(candidates.empty()) { return std::nullopt; }

        if(pendingGameId_)
        {
            const auto pending = std::find_if(candidates.begin(),
                                              candidates.end(),
                                              [this](const Candidate& candidate)
                                              {
                                                  return candidate.game.id == *pendingGameId_;
                                              });
            if(pending != candidates.end()) { return pending->game; }
        }

        const auto selected = std::min_element(candidates.begin(),
                                               candidates.end(),
                                               [](const Candidate& left, const Candidate& right)
                                               {
                                                   if(left.priority != right.priority)
                                                   {
                                                       return left.priority < right.priority;
                                                   }

                                                   return left.game.id < right.game.id;
                                               });

        return selected->game;
    }

    void SessionService::resetPendingStart() noexcept
    {
        pendingGameId_.reset();
        gameOpenDuration_ = seconds::zero();
    }
} // namespace gamelog::application::services
