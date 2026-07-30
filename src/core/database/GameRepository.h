#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QSqlDatabase>

#include "domain/Game.h"

namespace gamelog::core::database {

    /**
     * @brief Reads and writes rows in the games table.
     */
    class GameRepository
    {
    public:
        /**
         * @brief Binds the repository to one open database connection.
         */
        explicit GameRepository(const QSqlDatabase &database);

        /**
         * @brief Looks up one game by primary key.
         * @param id the sql table given game ID
         * @return A Game struct if found, a nullopt if not found.
         */
        [[nodiscard]] std::optional<domain::Game> findById(std::int64_t id) const;

        /**
         * @brief Looks up one game by name.
         * @param name the name of the game
         * @return A Game struct if found, a nullopt if not found.
         */
        [[nodiscard]] std::optional<domain::Game> findByName(const QString &name) const;

        /**
         * @brief Looks up one game by its executable path.
         * @param path the path to the game executable.
         * @return A Game struct if found, a nullopt if not.
         */
        [[nodiscard]] std::optional<domain::Game> findByPath(const QString &path) const;

        /**
         * @brief Returns every game ordered by title.
         * @return A List of Game structs containing all found games.
         */
        [[nodiscard]] std::vector<domain::Game> findAll() const;

        /**
         * @brief Inserts a new game row and updates the struct id.
         * @param game A Game to insert into the table.
         * @return a boolean describing the success of the insertion.
         */
        bool insert(domain::Game &game);

        /**
         * @brief Persists changes to an existing game row.
         * @param game a game to update inside the game table.
         */
        bool update(const domain::Game &game);

        /**
         * @brief Deletes a game row by primary key.
         * @param id a gameID to remove from the game table.
         */
        bool remove(std::int64_t id);

    private:
        QSqlDatabase database_;
    };

} // namespace gamelog::core::database
