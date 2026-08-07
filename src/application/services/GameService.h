#pragma once

#include <optional>
#include <vector>


#include "database/GameRepository.h"
#include "domain/Game.h"
#include "domain/query/GameQuery.h"

using std::vector;
using std::optional;
using gamelog::core::domain::Game;
using gamelog::core::domain::query::GameQuery;

namespace gamelog::application::services {

/**
 * Application-facing game operations.
 *
 * Callers describe general searches with GameQuery or use the semantic
 * convenience methods. SQL remains entirely behind GameRepository.
 */
class GameService : public QObject
{
Q_OBJECT
public:
    /**
     * Constructor for GameService
     * @param repository The database to pull games from
     */
    explicit GameService(core::database::GameRepository &repository);
    ~GameService() override = default;

    /**
     * Search the game Repository and return items specified according to the provided querey.
     * @param query A Querey object that specifies the parameters for the SQL querey that will be provided to the database.
     * @return
     */
    [[nodiscard]] vector<Game> search(const GameQuery &query) const;

    /**
     * Gets all the tracked games from the database.
     * @return A vector of Game objects.
     */
    [[nodiscard]] vector<Game> listTrackedGames() const;

    /**
     * Returns a game with a given ID.
     * @param id The id of the game to get.
     * @return The Game if found, null if not.
     */
    [[nodiscard]] optional<Game> findById(std::int64_t id) const;

    /**
     * Returns a game with a given executable name.
     * @param name The name of the executable to search for.
     * @return The Game if found, null if not.
     */
    [[nodiscard]] optional<Game> findByExecutableName(const QString &name) const;

    /**
     * Returns the game with the given executable path.
     * @param path The path to the executable of the desired Game.
     * @return The Game if found, null if not.
     */
    [[nodiscard]] optional<Game> findByExecutablePath(const QString &path) const;

    /**
     * Adds a new game to the database.
     * @param game The game to add.
     * @return true if the game was added successfully, false otherwise.
     */
    [[nodiscard]] bool addGame(Game &game);

    /**
     * Updates an existing game in the database.
     * @param game The game to update.
     * @return true if the game was updated successfully, false otherwise.
     */
    [[nodiscard]] bool updateGame(const Game &game);

    /**
     * Removes a game from the database.
     * @param id The ID of the game to remove.
     * @return true if the game was removed successfully, false otherwise.
     */
    [[nodiscard]] bool removeGame(std::int64_t id);

public slots:

    /**
     * Lists all games in the database and emits a GamesFound Signal with the results.
     */
    void listGames() const;

signals:

    /**
     * Emitted when a list of games is found.
     * @param games
     */
    void gamesFound(vector<Game> games) const;

private:
    /**
     * @brief The database to pull games from.
     */
    core::database::GameRepository &repository_;
};

} // namespace gamelog::application::services
