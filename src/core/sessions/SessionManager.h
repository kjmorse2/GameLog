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
        SessionManager(database::GameRepository &gameRepository,
                       database::SessionRepository &sessionRepository);

        /**
         * @brief Validates, inserts, and activates an automatic session.
         */
        [[nodiscard]] std::optional<domain::Session>
        startAutomaticSession(int gameId);

        /**
         * @brief Validates, inserts, and activates a manual session.
         */
        [[nodiscard]] std::optional<domain::Session>
        startManualSession(int gameId);

        /**
         * @brief Completes the active session and persists the existing row.
         *
         * If persistence fails, the original active state is retained so the
         * caller can retry without losing the session identity.
         */
        [[nodiscard]] std::optional<domain::Session> endActiveSession();

        /**
         * @brief Returns the in-memory active session, or a persisted active row.
         */
        [[nodiscard]] std::optional<domain::Session> activeSession() const;

        /**
         * @brief Reports whether this manager currently owns active in-memory state.
         */
        [[nodiscard]] bool hasActiveSession() const noexcept;

    private:
        [[nodiscard]] std::optional<domain::Session>
        startSession(int gameId, domain::SessionSource source);

        database::GameRepository &m_gameRepository;
        database::SessionRepository &m_sessionRepository;
        std::optional<domain::Session> m_activeSession;
    };

} // namespace gamelog::core::sessions
