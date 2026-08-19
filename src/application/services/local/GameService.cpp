#include "GameService.h"

#include "application/services/web/SteamApiService.h"
#include "application/services/web/SteamGameMapper.h"
#include "logging/LoggingCategories.h"

#include <utility>

#include <QSet>


namespace gamelog::application::services
{
    using core::domain::Game;
    using core::domain::query::GameQuery;

    namespace
    {
        template <typename T> std::optional<T> firstOrNull(std::vector<T> values)
        {
            if(values.empty()) { return std::nullopt; }
            return std::move(values.front());
        }
    } // namespace

    GameService::GameService(core::database::GameRepository& repository, SteamApiService& steamApiService)
        : repository_{repository},
          steamApiService_{steamApiService}
    {
        connect(&steamApiService_, &SteamApiService::ownedGamesReceived, this, &GameService::onSteamGamesReceived);
        qCInfo(gamelogGameServiceLog) << "Starting game service";
    }

    std::vector<Game> GameService::search(const GameQuery& query) const
    {
        qCDebug(gamelogGameServiceLog) << "Searching for games with provided query:" << query;
        return repository_.query(query);
    }

    std::vector<Game> GameService::listGames() const
    {
        qCDebug(gamelogGameServiceLog) << "Returning all games";
        return search({});
    }

    std::vector<Game> GameService::listTrackedGames() const
    {
        qCDebug(gamelogGameServiceLog) << "Returning all tracked games";
        GameQuery query;
        query.trackingEnabled = true;
        return search(query);
    }

    std::optional<Game> GameService::findById(int id) const
    {
        qCDebug(gamelogGameServiceLog) << "Returning game with id:" << id;
        GameQuery query;
        query.ids = {id};
        query.limit = 1;
        return firstOrNull(search(query));
    }

    std::optional<Game> GameService::findByExecutableName(const QString& name) const
    {
        qCDebug(gamelogGameServiceLog) << "Returning game with name:" << name;
        GameQuery query;
        query.executableName = name;
        query.limit = 1;
        return firstOrNull(search(query));
    }

    std::optional<Game> GameService::findByExecutablePath(const QString& path) const
    {
        qCDebug(gamelogGameServiceLog) << "Returning game with executable path:" << path;
        GameQuery query;
        query.executablePath = path;
        query.limit = 1;
        return firstOrNull(search(query));
    }

    bool GameService::addGame(Game& game)
    {
        qCDebug(gamelogGameServiceLog) << "Adding game:" << game;
        if(!repository_.insert(game)) { return false; }

        syncGamesWithDatabase();
        emit gameAdded(game);
        return true;
    }

    bool GameService::updateGame(const Game& game)
    {
        qCDebug(gamelogGameServiceLog) << "Updating game" << game;
        if(!repository_.update(game)) { return false; }

        syncGamesWithDatabase();
        emit gameUpdated(game);
        return true;
    }

    bool GameService::removeGame(int id)
    {
        qCDebug(gamelogGameServiceLog) << "Removing game with id:" << id;
        if(!repository_.remove(id)) { return false; }

        syncGamesWithDatabase();
        return true;
    }

    void GameService::syncGamesWithDatabase()
    {
        qCDebug(gamelogGameServiceLog) << "Syncing games with database";
        trackedPathGames_.clear();
        trackedSteamGames_.clear();

        for(const Game& game : listTrackedGames())
        {
            if(game.steamAppId && *game.steamAppId > 0)
            {
                trackedSteamGames_.insert(static_cast<std::uint32_t>(*game.steamAppId), game);
            }
            if(!game.executablePath.isEmpty())
            {
                // Executable paths are not yet unique in persistence. Preserve
                // the existing QHash replacement behavior until that schema
                // contract is deliberately changed.
                trackedPathGames_.insert(game.executablePath, game);
            }
        }

        qCInfo(gamelogGameServiceLog) << "Synced" << trackedSteamGames_.size() << "Steam games and" << trackedPathGames_
           .size() << "path-based games.";
    }

    const QHash<std::uint32_t, Game>& GameService::trackedSteamGames() const noexcept { return trackedSteamGames_; }

    const QHash<QString, Game>& GameService::trackedPathGames() const noexcept { return trackedPathGames_; }

    bool GameService::hasTrackedSteamGames() const noexcept { return !trackedSteamGames_.isEmpty(); }

    bool GameService::setHasArtwork(int gameId, bool available)
    {
        auto game = findById(gameId);
        if(!game) { return false; }

        if(game->hasArtwork == available) { return true; }

        game->hasArtwork = available;
        return updateGame(*game);
    }

    void GameService::syncSteamGames() { steamApiService_.getOwnedGames(); }

    void GameService::onSteamGamesReceived(const QJsonArray& steamGames)
    {
        qCInfo(gamelogGameServiceLog) << "Received" << steamGames.size() << "Steam games from API";

        // Resolve existing App IDs with one query rather than one per entry, and
        // rebuild the indexes once at the end rather than once per insert. Doing
        // either per game made a first-time sync of a large library quadratic.
        // Note this must consider the whole database, not just the tracked
        // index, so untracked rows are still recognized as existing.
        QSet<int> knownSteamAppIds;
        for(const Game& game : search({})) { if(game.steamAppId) { knownSteamAppIds.insert(*game.steamAppId); } }

        std::vector<Game> insertedGames;

        for(Game game : gamesFromSteamOwnedGames(steamGames))
        {
            // Synchronization never updates, re-enables, retitles, or duplicates
            // an App ID that already exists anywhere in the database.
            if(knownSteamAppIds.contains(*game.steamAppId)) { continue; }

            qCDebug(gamelogGameServiceLog) << "Adding game with Steam ID:" << *game.steamAppId;

            if(!repository_.insert(game))
            {
                qCWarning(gamelogGameServiceLog) << "Failed to add game with Steam ID:" << *game.steamAppId;
                continue;
            }

            // Guards against a duplicate App ID appearing twice in one payload.
            knownSteamAppIds.insert(*game.steamAppId);
            insertedGames.push_back(std::move(game));
        }

        if(insertedGames.empty()) { return; }

        syncGamesWithDatabase();

        // Emitted only after the indexes are consistent, so a handler that reads
        // them back sees every game from this batch.
        for(const Game& game : insertedGames) { emit gameAdded(game); }
    }
} // namespace gamelog::application::services
