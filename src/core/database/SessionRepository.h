#pragma once

#include <optional>
#include <vector>

#include <QSqlDatabase>

#include "domain/Session.h"

namespace gamelog::core::database {

    /**
     * @brief Reads and writes rows in the sessions table.
     */
    class SessionRepository
    {
    public:
        /**
         * @brief Binds the repository to one open database connection.
         * @param database the database to get the session table from.
         */
        explicit SessionRepository(const QSqlDatabase &database);

        /**
         * @brief Returns the single active session row, if one exists.
         * @return A Session struct if found, a nullopt if not found.
         */
        [[nodiscard]] std::optional<domain::Session> findActiveSession() const;

        /**
         * @brief Lists all sessions for one game, newest first.
         * @param gameId the sql ID of a game to get the session from
         * @return A vector of all the sessions for the given game.
         */
        [[nodiscard]] std::vector<domain::Session> listSessionsForGame(int gameId) const;

        /**
         * @brief Inserts a new session row and updates the struct id.
         * @param session the session struct to insert.
         * @return a boolean describing success.
         */
        [[nodiscard]] bool insert(domain::Session &session);

        /**
         * @brief Persists changes to exactly one existing session row.
         * @param session The session to update.
         * @return a boolean describing success.
         */
        [[nodiscard]] bool update(const domain::Session &session);

        /**
         * @brief Deletes exactly one session row by primary key.
         * @param sessionId the id of a session to remove.
         * @return a boolean describing success.
         */
        [[nodiscard]] bool remove(int sessionId);

    private:
        QSqlDatabase database_;
    };

} // namespace gamelog::core::database
