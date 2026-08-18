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
    class SessionService : public QObject
    {
        Q_OBJECT

    public:
        /**
         * Creates a SessionService with the provided repository and GameService.
         * @param repository The SessionRepository used for database operations.
         * @param gameService The GameService used for game-related operations.
         */
        SessionService(core::database::SessionRepository& repository, const GameService& gameService);

        ~SessionService() override = default;

        /**
         * Search the database for sessions matching the provided query.
         * @param query The query struct describing the search criteria.
         * @return A vector of Session objects returned from the query.
         */
        [[nodiscard]] std::vector<Session> search(const SessionQuery& query) const;

        /**
         * Search the database for the active session, if any.
         * @return The active session, or std::nullopt if there is no active session.
         */
        [[nodiscard]] std::optional<Session> findActiveSession() const;

        /**
         * Get all sessions for a specific game.
         * @param gameId The ID of the game to get sessions for.
         * @return A vector of Session objects for the specified game.
         */
        [[nodiscard]] std::vector<Session> listSessionsForGame(int gameId) const;

        /**
         * Get all sessions that started within the specified time range.
         * @param startDate The start of the time range (inclusive).
         * @param endDate The end of the time range (inclusive).
         * @return A vector of Session objects that started within the specified time range.
         */
        [[nodiscard]] std::vector<Session> getSessionsInTimeRange(const QDateTime& startDate,
                                                                  const QDateTime& endDate) const;

        /**
         * Starts a new session for the specified game, if there is no active session.
         * @param gameId The ID of the game to start a session for.
         * @return The new Session object if a session was started, or std::nullopt if there is already an active session.
         */
        [[nodiscard]] std::optional<Session> startAutomaticSession(int gameId);

        /**
         * End the active session, if any, and return it.
         * @return The ended Session object if there was an active session, or std::nullopt if there was no active session.
         */
        [[nodiscard]] std::optional<Session> endActiveSession();

        /**
         * Adds a new session to the database. This is used for manually created sessions.
         * @param session The Session object to add. The session ID will be set by the repository.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool addSession(Session& session);

        /**
         * Update an existing session in the database. This is used for manually updated sessions.
         * @param session The Session object to update. The session ID must be set to an existing session.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool updateSession(const Session& session);

        /**
         * Remove a session from the database. This is used for manually deleted sessions.
         * @param sessionId The ID of the session to remove.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool removeSession(int sessionId);

        /**
         * Restores the active session and its corresponding game from persistence.
         */
        [[nodiscard]] bool restoreActiveSession();

        /**
         * Advances automatic session detection using one process snapshot.
         */
        void updateAutomaticTracking(const std::vector<core::process::ProcessInfo>& processes,
                                     std::chrono::seconds elapsed);

        /**
         * Clears process-polling state without completing the active database row.
         */
        void resetAutomaticTracking() noexcept;

        signals  :

        /**
         * Emitted when a new session is started, either automatically or manually.
         * @param requestedGame The game for which the session was started.
         */
        void sessionStarted(const core::domain::Game& requestedGame);

        /**
         * Emitted when a session is stopped, either automatically or manually.
         * @param endedSession The session that was stopped, including its final duration and end time.
         */
        void sessionStopped(Session& endedSession);

    private:
        /**
         * Starts a new session for the specified game, if there is no active session. This is used internally by updateAutomaticTracking().
         * @param game The game for which to start a session.
         * @return The new Session object if a session was started, or std::nullopt if there is already an active session.
         */
        [[nodiscard]] std::optional<Session> startAutomaticSession(const core::domain::Game& game);

        /**
         * Reset the pending start state, clearing any pending game ID and resetting the grace period timer. This is used internally by
         * updateAutomaticTracking() when a session is stopped or when the pending start grace period expires.
         */
        void resetPendingStart() noexcept;

        /**
         * @breif The repository where the sessions are stored.
         */
        core::database::SessionRepository& repository_;

        /**
         * @breif The GameService used for game-related operations, such as looking up games by ID. This is a reference to an external service and is not owned by SessionService.
         */
        const GameService& gameService_;

        /**
         * @brief The currently active session, if any. This is used to track the session that is currently being recorded, and is updated when a session is started or stopped.
         */
        std::optional<Session> activeSession_;

        /**
         * @brief The active game for the currently active session, if any. This is used to track which game is currently being played, and is updated when a session is started or stopped.
         */
        std::optional<core::domain::Game> activeGame_;

        /**
         * @breif The game ID of the game that is pending to start a session. This is used to track which game is currently being played, and is updated when a session is started or stopped.
         */
        std::optional<int> pendingGameId_;

        /**
         * @breif The duration of time that the pending game has been open. This is used to determine when to start a new session for the pending game, and is updated by updateAutomaticTracking().
         */
        std::chrono::seconds gameOpenDuration_{0};

        /**
         * @brief The duration of time that the active game has been closed. This is used to determine when to stop the current session for the active game, and is updated by updateAutomaticTracking().
         */
        std::chrono::seconds gameClosedDuration_{0};

        /**
         * @brief The grace period for starting a new session
         */
        static constexpr std::chrono::seconds kStartGracePeriod{30};

        /**
         * @brief The grace period for ending a session
         */
        static constexpr std::chrono::seconds kEndGracePeriod{30};
    };
} // namespace gamelog::application::services
