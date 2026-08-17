#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QHash>
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
    class GameService:public QObject
    {
        Q_OBJECT

    public:
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
         * @param game the Game struct to add.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool updateGame(const Game& game);

        /**
         * removes the game with the provide id in the database and updates the in-memory indexes.
         * @param id of the Game struct to remove.
         * @return A boolean describing if the operation succeeded.
         */
        [[nodiscard]] bool removeGame(std::int64_t id);

        /**
         * Rebuilds the process-matching indexes from the repository.
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
         * @return a boolean describing if there are tracked team games.
         */
        [[nodiscard]] bool hasTrackedSteamGames() const noexcept;

        bool setHasArtwork(int gameId, bool available);

    public
        slots:


        void syncSteamGames();


        signals:




        void gameAdded(const Game&);

        void gameUpdated(const Game&);

    private:
        void onSteamGamesReceived(const QJsonArray& steamGames);

        /**
         * @breif the repository where the games are stored.
         */
        core::database::GameRepository& repository_;

        SteamApiService& steamApiService_;


        /**
         * The in-memory index of tracked Steam games, keyed by Steam App ID.
         */
        QHash<std::uint32_t, Game> trackedSteamGames_;

        /**
         * The in-memory index of tracked path games, keyed by executable path.
         */
        QHash<QString, Game> trackedPathGames_;
    };
} // namespace gamelog::application::services
