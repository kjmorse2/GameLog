#pragma once

#include <optional>

#include "domain/Session.h"

namespace gamelog::core::database {
    class GameRepository;
    class SessionRepository;
} // namespace gamelog::core::database

namespace gamelog::core::sessions {

    /**
     * @brief Owns the in-memory active-session state and its persistence boundary.
     *
     * The repository references are required, non-owning dependencies. They must
     * outlive this manager.
     */
    class SessionManager
    {
    public:
        /**
         * @brief Constructs a new session manager.
         * @param gameRepository The interface for accessing the games table of the database.
         * @param sessionRepository The interface for accessing the session table of the database.
         */
        SessionManager(database::GameRepository &gameRepository, database::SessionRepository &sessionRepository);

        /**
         * @brief Validates, inserts, and activates an automatic session.
         * @param gameId the id of the game to start the session for.
         * @return The new session if one was created
         */
        [[nodiscard]] std::optional<domain::Session> startAutomaticSession(int gameId);

        /**
         * @brief Validates, inserts, and activates a manual session.
         * @param gameId the id of the game to start the session for.
         * @return The new session if one was created
         */
        [[nodiscard]] std::optional<domain::Session> startManualSession(int gameId);

        /**
         * @brief Completes the active session and persists the existing row.
         *
         * If persistence fails, the original active state is retained so the
         * caller can retry without losing the session identity.
         */
        [[nodiscard]] std::optional<domain::Session> endActiveSession();

        /**
         * @brief Returns the in-memory active session, or a persisted active row.
         * @return The active session if there is one.
         */
        [[nodiscard]] std::optional<domain::Session> activeSession() const;

        /**
         * @brief Reports whether this manager currently owns active in-memory state.
         * @return a boolean describing if there is an active session.
         */
        [[nodiscard]] bool hasActiveSession() const noexcept;

    private:
        /**
         * @brief Starts a new session.
         * @param gameId the database id of the game to start the session for.
         * @param source enum describing the source of the session.
         * @return The new session if one was created.
         */
        [[nodiscard]] std::optional<domain::Session> startSession(int gameId, domain::SessionSource source);

        /**
         * @brief The game table interface.
         */
        database::GameRepository &m_gameRepository;

        /**
         * @brief The session table interface.
         */
        database::SessionRepository &m_sessionRepository;

        /**
         * @brief The active session if there is one.
         */
        std::optional<domain::Session> m_activeSession;
    };

} // namespace gamelog::core::sessions
