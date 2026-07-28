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

    /**
     * @brief Constructs a SessionManager with the given GameRepository and SessionRepository.
     * @param gameRepository The GameRepository instance to be used for game-related database operations.
     * @param sessionRepository The SessionRepository instance to be used for session-related database operations.
     */
    SessionManager(database::GameRepository &gameRepository, database::SessionRepository &sessionRepository);

    /**
     * @brief Starts an automatic session for the specified game.
     * @param gameId The ID of the game for which to start the session.
     * @return An optional containing the started session, or std::nullopt if the session could not be started.
     */
    [[nodiscard]] std::optional<domain::Session> startAutomaticSession(int gameId);

    /**
     * @brief Ends the currently active session, if any.
     * @return True if the active session was successfully ended, false otherwise.
     */
    [[nodiscard]] std::optional<domain::Session> startManualSession(int gameId);

    /**
     * @brief Ends the currently active session, if any.
     * @return True if the active session was successfully ended, false otherwise.
     */
    [[nodiscard]] bool endActiveSession();

    /**
     * @brief Retrieves the currently active session, if any.
     * @return An optional containing the active session, or std::nullopt if no active session is currently in progress.
     */
    [[nodiscard]] std::optional<domain::Session> activeSession();

    /**
     * @brief Checks if there is an active session in progress.
     * @return True if there is an active session, false otherwise.
     */
    [[nodiscard]] bool sessionExists();

private:
    /**
     * @brief The GameRepository instance used for game-related database operations.
     */
    database::GameRepository &m_gameRepository;

    /**
     * @brief The SessionRepository instance used for session-related database operations.
     */
    database::SessionRepository &m_sessionRepository;

    /**
     * @brief A flag indicating whether there is an active session in progress.
     */
    bool m_isSessionActive{false};

    /**
     * @brief The currently active session, if any.
     */
    domain::Session m_activeSession;

    /**
     * @brief The currently active game associated with the active session, if any.
     */
    domain::Game m_activeGame;
};
} // namespace gamelog::core::sessions
