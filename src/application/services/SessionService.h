#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <QObject>

#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "domain/Session.h"
#include "domain/query/SessionQuery.h"
#include "process/ProcessInfo.h"

namespace gamelog::application::services
{
    class GameService;

    /**
     * Application-facing session operations and automatic-session lifecycle.
     * Owns all in-memory state associated with the current/pending session.
     */
    class SessionService:public QObject
    {
        Q_OBJECT

    public:
        SessionService(core::database::SessionRepository& repository, const GameService& gameService);

        ~SessionService() override = default;

        [[nodiscard]] std::vector<core::domain::Session> search(const core::domain::query::SessionQuery& query) const;

        [[nodiscard]] std::optional<core::domain::Session> findActiveSession() const;

        [[nodiscard]] std::vector<core::domain::Session> listSessionsForGame(int gameId) const;

        [[nodiscard]] std::vector<core::domain::Session> getSessionsInTimeRange(const QDateTime& startDate, const QDateTime& endDate) const;

        [[nodiscard]] std::optional<core::domain::Session> startAutomaticSession(int gameId);

        [[nodiscard]] std::optional<core::domain::Session> endActiveSession();

        [[nodiscard]] bool addSession(core::domain::Session& session);

        [[nodiscard]] bool updateSession(const core::domain::Session& session);

        [[nodiscard]] bool removeSession(int sessionId);

        /**
         * Restores the active session and its corresponding game from persistence.
         */
        [[nodiscard]] bool restoreActiveSession();

        /**
         * Advances automatic session detection using one process snapshot.
         */
        void updateAutomaticTracking(const std::vector<core::process::ProcessInfo>& processes, std::chrono::seconds elapsed);

        /**
         * Clears process-polling state without completing the active database row.
         */
        void resetAutomaticTracking() noexcept;

        signals:



        void sessionStarted(const core::domain::Game& requestedGame);

        void sessionStopped(Session& endedSession);

    private:
        [[nodiscard]] std::optional<Session> startAutomaticSession(const core::domain::Game& game);

        void resetPendingStart() noexcept;

        core::database::SessionRepository& repository_;
        const GameService& gameService_;

        std::optional<Session> activeSession_;
        std::optional<core::domain::Game> activeGame_;
        std::optional<int> pendingGameId_;

        std::chrono::seconds gameOpenDuration_{0};
        std::chrono::seconds gameClosedDuration_{0};

        static constexpr std::chrono::seconds kStartGracePeriod{30};
        static constexpr std::chrono::seconds kEndGracePeriod{30};
    };
} // namespace gamelog::application::services
