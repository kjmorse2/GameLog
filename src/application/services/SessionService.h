#pragma once

#include <optional>
#include <vector>

#include "database/SessionRepository.h"
#include "domain/Game.h"
#include "domain/Session.h"
#include "domain/query/SessionQuery.h"

namespace gamelog::application::services {

class GameService;


/**
 * Application-facing session operations and lifecycle facade.
 */
class SessionService : public QObject
{
Q_OBJECT
public:
    SessionService(
        core::database::SessionRepository &repository,
        const GameService &gameService);

    [[nodiscard]] std::vector<core::domain::Session>
    search(const core::domain::query::SessionQuery &query) const;

    [[nodiscard]] std::optional<core::domain::Session> findActiveSession() const;
    [[nodiscard]] std::vector<core::domain::Session>
    listSessionsForGame(int gameId) const;

    [[nodiscard]] std::optional<core::domain::Session>
    startAutomaticSession(int gameId);
    [[nodiscard]] std::optional<core::domain::Session> endActiveSession();

    [[nodiscard]] bool addSession(core::domain::Session &session);
    [[nodiscard]] bool updateSession(const core::domain::Session &session);
    [[nodiscard]] bool removeSession(int sessionId);

signals:
    void sessionStarted(core::domain::Game requestedGame);
    void sessionStopped();

private:
    core::database::SessionRepository &repository_;
    const GameService &gameService_;
    std::optional<core::domain::Session> activeSession_;

    void restoreActiveSession();
};

} // namespace gamelog::application::services
