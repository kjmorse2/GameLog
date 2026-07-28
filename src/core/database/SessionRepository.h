#pragma once

#include <optional>
#include <vector>
#include <QtSql/qsqldatabase.h>
#include "domain/Session.h"

namespace gamelog::core::database
{
/**
 * @brief The SessionRepository class provides an interface for managing session records in a database.
 */
class SessionRepository
{
public:
    /**
     * @brief Constructs a SessionRepository with the given QSqlDatabase.
     * @param database The QSqlDatabase instance to be used for database operations.
     */
    explicit SessionRepository(QSqlDatabase database);

    /**
     * @brief Finds the active session in the database.
     * @return An optional containing the active session, or std::nullopt if no active session is found.
     */
    [[nodiscard]] std::optional<domain::Session> findActiveSession();

    /**
     * @brief Finds a session by its ID.
     * @param id The ID of the session to find.
     * @return An optional containing the found session, or std::nullopt if not found
     */
    [[nodiscard]] std::vector<domain::Session> listSessionsForGame(int gameId);

    /**
     * @brief Inserts a new session into the database.
     * @param session The session to insert. The ID will be set upon successful insertion.
     * @return True if the insertion was successful, false otherwise.
     */
    bool insert(domain::Session& session);

    /**
     * @brief Updates an existing session in the database.
     * @param session The session to update. The ID must be set to identify the session.
     * @return True if the update was successful, false otherwise.
     */
    bool update(const domain::Session& session);

    /**
     * @brief Removes a session from the database by its ID.
     * @param sessionId The ID of the session to remove.
     * @return True if the removal was successful, false otherwise.
     */
    bool remove(int sessionId);

private:
    /**
     * @brief The QSqlDatabase instance used for database operations.
     */
    QSqlDatabase database_;
};
} // namespace gamelog::core::database
