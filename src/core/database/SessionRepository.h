#pragma once

#include <vector>

#include <QSqlDatabase>

#include "domain/Session.h"
#include "domain/query/SessionQuery.h"

using std::vector;
using gamelog::core::domain::Session;
using gamelog::core::domain::query::SessionQuery;

namespace gamelog::core::database
{
    /**
 * Translates SessionQuery specifications into SQL and persists Session rows.
 */
    class SessionRepository
    {
    public:
        /**
     * Constructs a SessionRepository.
     * @param database The database to get sessions from.
     */
        explicit SessionRepository(const QSqlDatabase& database);

        /**
     * Queries the database for sessions matching the given specification.
     * @param specification The query specification.
     * @return A vector of sessions matching the specification.
     */
        [[nodiscard]] vector<Session> query(const SessionQuery& specification) const;

        /**
     * Add a session to the repository.
     * @param session The session to add.
     * @return True if the session was added, false otherwise.
     */
        [[nodiscard]] bool insert(Session& session);

        /**
     * Update a session in the repository.
     * @param session The session to update.
     * @return true if the session was updated.
     */
        [[nodiscard]] bool update(const Session& session);

        /**
     * Remove a session from the repository.
     * @param sessionId The id of the session to remove.
     * @return True if the session was removed, false otherwise.
     */
        [[nodiscard]] bool remove(int sessionId);

    private:
        /**
     * @brief The database to get sessions from.
     */
        QSqlDatabase database_;
    };
} // namespace gamelog::core::database
