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
         */
        explicit SessionRepository(QSqlDatabase database);

        /**
         * @brief Returns the single active session row, if one exists.
         */
        [[nodiscard]] std::optional<domain::Session> findActiveSession() const;

        /**
         * @brief Lists all sessions for one game, newest first.
         */
        [[nodiscard]] std::vector<domain::Session>
        listSessionsForGame(int gameId) const;

        /**
         * @brief Inserts a new session row and updates the struct id.
         */
        bool insert(domain::Session &session);

        /**
         * @brief Persists changes to an existing session row.
         */
        bool update(const domain::Session &session);

        /**
         * @brief Deletes a session row by primary key.
         */
        bool remove(int sessionId);

    private:
        QSqlDatabase database_;
    };

} // namespace gamelog::core::database
