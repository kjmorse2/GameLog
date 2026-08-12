#pragma once

#include <optional>
#include <vector>

#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "domain/Session.h"
#include "domain/query/SessionQuery.h"

using std::vector;
using std::optional;
using gamelog::core::domain::Session;
using gamelog::core::domain::Game;
using gamelog::core::domain::query::SessionQuery;
using gamelog::core::database::SessionRepository;

namespace gamelog::application::services
{
    class GameService;


    /**
 * Application-facing session operations and lifecycle facade.
 */
    class SessionService : public QObject
    {
        Q_OBJECT

    public:
        SessionService(SessionRepository& repository, const GameService& gameService);

        [[nodiscard]] vector<Session> search(const SessionQuery& query) const;

        [[nodiscard]] optional<Session> findActiveSession() const;

        [[nodiscard]] vector<Session> listSessionsForGame(int gameId) const;

        [[nodiscard]] optional<Session> startAutomaticSession(int gameId);

        [[nodiscard]] optional<Session> endActiveSession();

        [[nodiscard]] bool addSession(Session& session);

        [[nodiscard]] bool updateSession(const Session& session);

        [[nodiscard]] bool removeSession(int sessionId);

    public
        slots:

        [[nodiscard]] vector<Session> getSessionsInTimeRange(const QDateTime& startDate, const QDateTime& endDate) const;

        signals:

        void sessionStarted(Game requestedGame);

        void sessionStopped();

    private:
        SessionRepository& repository_;
        const GameService& gameService_;
        optional<Session> activeSession_;

        void restoreActiveSession();
    };
} // namespace gamelog::application::services
