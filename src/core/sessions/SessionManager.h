#pragma once

#include <optional>

#include "database/GameRepository.h"
#include "database/SessionRepository.h"
#include "domain/Session.h"

namespace gamelog::core::sessions
{
class SessionManager
{
public:
    SessionManager(database::GameRepository &gameRepository, database::SessionRepository &sessionRepository);

    [[nodiscard]] std::optional<domain::Session> startAutomaticSession(int gameId);
    [[nodiscard]] std::optional<domain::Session> startManualSession(int gameId);
    [[nodiscard]] bool endActiveSession();
    [[nodiscard]] std::optional<domain::Session> activeSession();

private:
    database::GameRepository &m_gameRepository;
    database::SessionRepository &m_sessionRepository;
};
} // namespace gamelog::core::sessions
