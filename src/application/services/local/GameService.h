#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QString>

#include "database/GameRepository.h"
#include "domain/Game.h"
#include "domain/query/GameQuery.h"

namespace gamelog::application::services
{
    class SteamApiService;

    /**
     * Application-facing game operations.
     *
     * Callers describe general searches with GameQuery or use the semantic
     * convenience methods. SQL remains entirely behind GameRepository.
     * The service also owns the in-memory index used by process matching.
     */
    class GameService : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief Constructs a GameService with the provided repository and SteamApiService.
         * @param repository The GameRepository used for database operations.
         * @param steamApiService The SteamApiService used for Steam-related operations.
         */
        explicit GameService(core::database::GameRepository& repository, SteamApiService& steamApiService);

        ~GameService() override = default;

        /**
         * Query the database with the provided query struct and return the results.
         * @param query The query struct describing the search criteria.
         * @return A vector of Game objects returned from the query.
         */
        [[nodiscard]] std::vector<Game> search(const GameQuery& query) const;

        /**
         * Returns all games in the database.
         * @return A vector of all games.
         */
        [[nodiscard]] std::vector<Game> listGames() const;

        /**
         * Returns all tracked games in the database.
         * @return A vector of all tracked games.
         */
        [[nodiscard]] std::vector<Game> listTrackedGames() const;

        /**
         * Gets the game with the specified ID from the database if it exists.
         * @param id of the game to find
         * @return The Game struct if found, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<Game> findById(std::int64_t id) const;

        /**
         * Gets the game with the specified executable name from the database if it exists.
         * @param name of the executable.
         * @return The Game struct if found, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<Game> findByExecutableName(const QString& name) const;

        /**
         * Gets the game with the specified executable path from the database if it exists.
         * @param path to the executable.
         * @return The Game struct if found, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<Game> findByExecutablePath(const QString& path) const;

        /**
         * Adds a new game to the database and updates the in-memory indexes.
         * @param game The Game struct to add.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool addGame(Game& game);

        /**
         * Updates the provided game in the database and updates the in-memory indexes.
         * @param game the Game struct to update.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool updateGame(const Game& game);

        /**
         * Removes the game with the provided id in the database and updates the in-memory indexes.
         * @param id of the Game struct to remove.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool removeGame(std::int64_t id);

        /**
         * Rebuilds the process-matching indexes from the repository. Steam App
         * IDs are unique in persistence. Duplicate executable-path behavior is
         * intentionally not strengthened by this contract revision.
         */
        void syncGamesWithDatabase();

        /**
         * Read-only access to the service-owned process-matching indexes for Steam games.
         * References remain valid until the next cache refresh.
         * @return The tracked Steam games indexed by Steam App ID.
         */
        [[nodiscard]] const QHash<std::uint32_t, Game>& trackedSteamGames() const noexcept;

        /**
         * Read-only access to the service-owned process-matching indexes for path tracked games.
         * @return The tracked path games indexed by executable path.
         */
        [[nodiscard]] const QHash<QString, Game>& trackedPathGames() const noexcept;

        /**
         * Returns true if there are any tracked Steam games in the in-memory index.
         * @return a boolean describing if there are tracked Steam games.
         */
        [[nodiscard]] bool hasTrackedSteamGames() const noexcept;

        /**
         * Persists the current cover-artwork availability for one game.
         * @param gameId The persisted game ID.
         * @param available Whether valid cover artwork exists locally.
         * @return True if the game exists and the state was persisted.
         */
        [[nodiscard]] bool setHasArtwork(int gameId, bool available);

    public
        slots :
        /**
         * Wrapper for SteamApiService::getOwnedGames() that updates the database when results arrive.
         */
        void syncSteamGames();

        signals :
        /**
         * Emitted when a new game is added to the database and the in-memory indexes are updated.
         * @param game The Game struct that was added.
         */
        void gameAdded(const Game& game);

        /**
         * Emitted when a game is updated in the database and the in-memory indexes are updated.
         * @param game The Game struct that was updated.
         */
        void gameUpdated(const Game& game);

    private:
        /**
         * @brief the repository where the games are stored.
         */
        core::database::GameRepository& repository_;

        /**
         * @brief the SteamApiService used for Steam-related operations.
         */
        SteamApiService& steamApiService_;

        /**
         * The in-memory index of tracked Steam games, keyed by Steam App ID.
         */
        QHash<std::uint32_t, Game> trackedSteamGames_;

        /**
         * The in-memory index of tracked path games, keyed by executable path.
         */
        QHash<QString, Game> trackedPathGames_;

    private
        slots :
        /**
         * Connected to the SteamApiService::ownedGamesReceived signal. Inserts
         * only Steam App IDs that do not already exist anywhere in the database.
         * Existing rows are left unchanged, including untracked rows and local titles.
         * @param steamGames The array of Steam games received from the Steam API.
         */
        void onSteamGamesReceived(const QJsonArray& steamGames);
    };
} // namespace gamelog::application::services
