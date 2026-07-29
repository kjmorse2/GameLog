#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QSqlDatabase>

#include "domain/Game.h"

namespace gamelog::core::database
{

/**
 * @brief Reads and writes rows in the games table.
 */
class GameRepository
{
public:
    /**
     * @brief Binds the repository to one open database connection.
     */
    explicit GameRepository(QSqlDatabase database);

    /**
     * @brief Looks up one game by primary key.
     */
    [[nodiscard]] std::optional<domain::Game>
    findById(std::int64_t id) const;

    /**
     * @brief Returns every game ordered by title.
     */
    [[nodiscard]] std::vector<domain::Game>
    findAll() const;

    /**
     * @brief Inserts a new game row and updates the struct id.
     */
    bool insert(domain::Game& game);

    /**
     * @brief Persists changes to an existing game row.
     */
    bool update(const domain::Game& game);

    /**
     * @brief Deletes a game row by primary key.
     */
    bool remove(std::int64_t id);

private:
    QSqlDatabase database_;
};

} // namespace gamelog::core::database
