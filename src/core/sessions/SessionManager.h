#pragma once

#include <optional>

#include "domain/Game.h"
#include "domain/Session.h"

namespace gamelog::core::database
{
class GameRepository;
class SessionRepository;
}

namespace gamelog::core::sessions
{
/**
 * @brief Coordinates the active in-memory session state.
 */
class SessionManager
{
public:

    /**
     * @brief Connects the manager to the repositories it needs.
     */
    SessionManager(database::GameRepository &gameRepository, database::SessionRepository &sessionRepository);

    /**
     * @brief Starts an automatic session for the given game.
     */
    [[nodiscard]] std::optional<domain::Session> startAutomaticSession(int gameId);

    /**
     * @brief Starts a manual session for the given game.
     */
    [[nodiscard]] std::optional<domain::Session> startManualSession(int gameId);

    /**
     * @brief Ends the current in-memory active session, if one exists.
     */
    [[nodiscard]] bool endActiveSession();

    /**
     * @brief Returns the in-memory active session or a persisted fallback.
     */
    [[nodiscard]] std::optional<domain::Session> activeSession();

private:
    /**
     * @brief Repository for game lookups.
     */
    database::GameRepository &m_gameRepository;

    /**
     * @brief Repository for session persistence.
     */
    database::SessionRepository &m_sessionRepository;

    /**
     * @brief Mirrors whether m_activeSession currently represents live state.
     */
    bool m_isSessionActive{false};

    /**
     * @brief Cached active session while the manager owns the lifecycle.
     */
    domain::Session m_activeSession;

    /**
     * @brief Cached game associated with the active session.
     */
    domain::Game m_activeGame;
};
} // namespace gamelog::core::sessions
