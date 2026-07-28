#pragma once

#include <optional>
#include <vector>

#include "domain/Game.h"
#include <QtSql/qsqldatabase.h>

namespace gamelog::core::database
{
/**
 * @brief The GameRepository class provides an interface for managing game records in a database.
 */
class GameRepository
{
public:
    /**
     * @brief Constructs a GameRepository with the given QSqlDatabase.
     * @param database The QSqlDatabase instance to be used for database operations.
     */
    explicit GameRepository(QSqlDatabase database);

    /**
     * @brief Finds a game by its ID.
     * @param id The ID of the game to find.
     * @return An optional containing the found game, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<domain::Game> findById(std::int64_t id) const;

    /**
     * @brief Finds all games in the database.
     * @return A vector containing all games in the database.
     */
    [[nodiscard]] std::vector<domain::Game> findAll() const;

    /**
     * @brief Inserts a new game into the database.
     * @param game The game to insert. The ID will be set upon successful insertion.
     * @return True if the insertion was successful, false otherwise.
     */
    bool insert(domain::Game& game);

    /**
     * @brief Updates an existing game in the database.
     * @param game The game to update. The ID must be set to identify the game.
     * @return True if the update was successful, false otherwise.
     */
    bool update(const domain::Game& game);

    /**
     * @brief Removes a game from the database by its ID.
     * @param id The ID of the game to remove.
     * @return True if the removal was successful, false otherwise.
     */
    bool remove(std::int64_t id);

private:
    /**
     * @brief The QSqlDatabase instance used for database operations.
     */
    QSqlDatabase database_;
};
} // namespace gamelog::core::database
