#pragma once

#include <vector>

#include <QSqlDatabase>

#include "domain/Game.h"
#include "domain/query/GameQuery.h"

namespace gamelog::core::database
{
    /**
     * Translates GameQuery specifications into SQL and persists Game rows.
     */
    class GameRepository
    {
    public:
        /**
         * Constructs a GameRepository.
         * @param database The database to read from.
         */
        explicit GameRepository(const QSqlDatabase& database);

        /**
         * Queries the database for games matching the given specification.
         * @param specification The query specification.
         * @return A vector of games matching the specification.
         */
        [[nodiscard]] std::vector<domain::Game> query(const domain::query::GameQuery& specification) const;

        /**
         * Inserts a game into the database. The game must have ID zero, a
         * nonblank title, and either no Steam App ID or a positive Steam App ID.
         * @param game The game to insert. Its generated ID is assigned on success.
         * @return True if the game was inserted, false otherwise.
         */
        [[nodiscard]] bool insert(domain::Game& game);

        /**
         * Updates a persisted game. The game must have a positive ID, a
         * nonblank title, and either no Steam App ID or a positive Steam App ID.
         * @param game The game to update.
         * @return True if exactly one game was updated, false otherwise.
         */
        [[nodiscard]] bool update(const domain::Game& game);

        /**
         * Removes a Game from the database.
         * @param id The ID of the game to remove.
         * @return True if exactly one game was removed, false otherwise.
         */
        [[nodiscard]] bool remove(int id);

    private:
        QSqlDatabase database_;
    };
} // namespace gamelog::core::database
