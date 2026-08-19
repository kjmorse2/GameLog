#pragma once

#include <vector>

#include <QSqlDatabase>

#include "domain/Session.h"
#include "domain/query/SessionQuery.h"

namespace gamelog::core::database
{
    /**
     * Translates SessionQuery specifications into SQL and persists Session rows.
     * The repository is the final persistence boundary for Session state
     * invariants and keeps session rows atomic with their note documents.
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
         * Persisted notes are loaded with each Session. Individually corrupted
         * rows are logged and skipped without discarding valid rows.
         * @param specification The query specification.
         * @return A vector of sessions matching the specification.
         */
        [[nodiscard]] std::vector<domain::Session> query(const domain::query::SessionQuery& specification) const;

        /**
         * Add a valid session and its note document in one transaction. Every
         * session receives a document row, even when notes are empty. On a
         * rollback after ID assignment, session.id is restored to zero.
         * @param session The session to add.
         * @return True if the session was added, false otherwise.
         */
        [[nodiscard]] bool insert(domain::Session& session);

        /**
         * Update a valid session and any changed note content in one transaction.
         * The document timestamp changes only when note content changes or a
         * missing document row must be created.
         * @param session The session to update.
         * @return true if exactly one session was updated atomically.
         */
        [[nodiscard]] bool update(const domain::Session& session);

        /**
         * Remove a session from the repository.
         * @param sessionId The id of the session to remove.
         * @return True if exactly one session was removed, false otherwise.
         */
        [[nodiscard]] bool remove(int sessionId) const;

    private:
        /**
         * @brief The database to get sessions from.
         */
        QSqlDatabase database_;
    };
} // namespace gamelog::core::database
