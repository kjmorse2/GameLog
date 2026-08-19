#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

#include <QDateTime>
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
        using Clock = std::function<QDateTime()>;

        /**
         * Creates a SessionService with the provided repository and GameService.
         * Production lifecycle timestamps use QDateTime::currentDateTimeUtc().
         * @param repository The SessionRepository used for database operations.
         * @param gameService The GameService used for game-related operations.
         */
        SessionService(core::database::SessionRepository& repository, const GameService& gameService);

        /**
         * Creates a SessionService with a caller-provided clock.
         * This narrow seam keeps time-dependent tests deterministic without
         * changing production behavior.
         * @param repository The SessionRepository used for database operations.
         * @param gameService The GameService used for game-related operations.
         * @param clock A function returning the current date and time.
         */
        SessionService(core::database::SessionRepository& repository, const GameService& gameService, Clock clock);

        ~SessionService() override = default;

        /**
         * Search the database for sessions matching the provided query.
         * @param query The query struct describing the search criteria.
         * @return A vector of Session objects returned from the query.
         */
        [[nodiscard]] std::vector<core::domain::Session> search(const core::domain::query::SessionQuery& query) const;

        /**
         * Search the cached service state for the active session, if any.
         * @return The active session, or std::nullopt if there is no active session.
         */
        [[nodiscard]] std::optional<core::domain::Session> findActiveSession() const;

        /**
         * Get all sessions for a specific game.
         * @param gameId The ID of the game to get sessions for.
         * @return A vector of Session objects for the specified game.
         */
        [[nodiscard]] std::vector<core::domain::Session> listSessionsForGame(int gameId) const;

        /**
         * Get all sessions that started within the half-open time range.
         * @param startDate The start of the time range (inclusive).
         * @param endDate The end of the time range (exclusive).
         * @return Sessions whose start timestamps are in [startDate, endDate).
         */
        [[nodiscard]] std::vector<core::domain::Session> getSessionsInTimeRange(
            const QDateTime& startDate,
            const QDateTime& endDate) const;

        /**
         * Starts a new automatic session for a tracked game if no session is active.
         * Games with tracking disabled are rejected.
         * @param gameId The ID of the game to start a session for.
         * @return The new Session, or std::nullopt if the operation is rejected.
         */
        [[nodiscard]] std::optional<core::domain::Session> startAutomaticSession(int gameId);

        /**
         * Ends the active session, replacing its tracked duration with the
         * wall-clock difference between its start and end timestamps.
         * A future start timestamp causes the operation to fail.
         * @return The ended Session, or std::nullopt if no valid session can be ended.
         */
        [[nodiscard]] std::optional<core::domain::Session> endActiveSession();

        /**
         * Adds a new session to the database. This is used for manually created sessions.
         * Adding an active session emits sessionStarted() after persistence succeeds.
         * @param session The Session to add. Its ID is assigned by the repository.
         * @return True if the operation succeeded.
         */
        [[nodiscard]] bool addSession(core::domain::Session& session);

        /**
         * Updates an existing session in the database. Lifecycle signals are
         * emitted only when its status crosses the active/inactive boundary.
         * @param session The Session to update. Its ID must identify an existing row.
         * @return True if the operation succeeded.
         */
        [[nodiscard]] bool updateSession(const core::domain::Session& session);

        /**
         * Removes an inactive session from the database. Active sessions must
         * first be completed or interrupted and therefore cannot be removed.
         * @param sessionId The ID of the session to remove.
         * @return True if the operation succeeded.
         */
        [[nodiscard]] bool removeSession(int sessionId);

        /**
         * Restores persisted active state. If multiple active rows exist, the
         * newest restorable row is retained and every other row is interrupted.
         * Active rows referencing missing games are also interrupted.
         * @return True when restoration and any required repair both succeed.
         */
        [[nodiscard]] bool restoreActiveSession();

        /**
         * Advances automatic session detection using one deterministic process snapshot.
         */
        void updateAutomaticTracking(const std::vector<core::process::ProcessInfo>& processes,
                                     std::chrono::seconds elapsed);

        /**
         * Clears process-polling state without completing the active database row.
         */
        void resetAutomaticTracking() noexcept;

    Q_SIGNALS:
        /**
         * Emitted after a database row changes from inactive/nonexistent to active.
         * @param requestedGame The game for which the session was started.
         */
        void sessionStarted(const core::domain::Game& requestedGame);

        /**
         * Emitted after a database row changes from active to completed or interrupted.
         * The value parameter is safe for queued connections and is not an in/out hook.
         * @param endedSession The final persisted session value.
         */
        void sessionStopped(core::domain::Session endedSession);

    private:
        /**
         * Starts a new automatic session for the supplied game.
         * @param game The game for which to start a session.
         * @return The new Session, or std::nullopt if the operation is rejected.
         */
        [[nodiscard]] std::optional<core::domain::Session> startAutomaticSession(const core::domain::Game& game);

        /**
         * Returns the current time from the configured clock in UTC.
         */
        [[nodiscard]] QDateTime currentDateTimeUtc() const;

        /**
         * Returns whether persistence contains an active row other than the
         * optionally excluded session ID.
         */
        [[nodiscard]] bool hasOtherActiveSession(int excludedSessionId = 0) const;

        /**
         * Interrupts an active row using a valid end timestamp and duration.
         */
        [[nodiscard]] bool interruptSession(core::domain::Session session, const QDateTime& interruptedAt);

        /**
         * Chooses one game from a process snapshot deterministically. A still
         * detected pending game is retained; otherwise Steam matches precede
         * path-only matches, with game ID used as a tie-breaker.
         */
        [[nodiscard]] std::optional<core::domain::Game> selectDetectedGame(
            const std::vector<core::process::ProcessInfo>& processes) const;

        /**
         * Reset the pending start state, clearing any pending game ID and
         * resetting the grace-period timer.
         */
        void resetPendingStart() noexcept;

        /**
         * @brief The repository where sessions are stored.
         */
        core::database::SessionRepository& repository_;

        /**
         * @brief The external GameService used for game lookups and tracking indexes.
         */
        const GameService& gameService_;

        /**
         * @brief Clock used for lifecycle timestamps.
         */
        Clock clock_;

        /**
         * @brief The currently active session, if any.
         */
        std::optional<core::domain::Session> activeSession_;

        /**
         * @brief The game associated with the current active session, if available.
         */
        std::optional<core::domain::Game> activeGame_;

        /**
         * @brief The game ID currently accumulating the automatic-start grace period.
         */
        std::optional<int> pendingGameId_;

        /**
         * @brief The amount of time the pending game has remained detected.
         */
        std::chrono::seconds gameOpenDuration_{0};

        /**
         * @brief The amount of time the active game has remained undetected.
         */
        std::chrono::seconds gameClosedDuration_{0};

        /**
         * @brief The grace period for starting a new automatic session.
         */
        static constexpr std::chrono::seconds kStartGracePeriod{30};

        /**
         * @brief The grace period for ending an automatic session.
         */
        static constexpr std::chrono::seconds kEndGracePeriod{30};
    };
} // namespace gamelog::application::services
