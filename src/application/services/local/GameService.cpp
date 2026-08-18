#include "GameService.h"

#include <QJsonObject>
#include <utility>
#include <application/services/web/SteamApiService.h>

#include "../web/GameArtworkService.h"
#include "logging/LoggingCategories.h"

namespace gamelog::application::services
{
    namespace
    {
        template <typename T> std::optional<T> firstOrNull(std::vector<T> values)
        {
            if(values.empty()) { return std::nullopt; }
            return std::move(values.front());
        }
    } // namespace

    GameService::GameService(core::database::GameRepository& repository, SteamApiService& steamApiService) :
        repository_{repository},
        steamApiService_{steamApiService}
    {
        connect(&steamApiService_, &SteamApiService::ownedGamesReceived, this, &GameService::onSteamGamesReceived);
        qCInfo(gamelogRuntimeLog) << "Starting game service";
    }

    std::vector<Game> GameService::search(const GameQuery& query) const
    {
        qCDebug(gamelogRuntimeLog) << "Searching for games with provided query:" << query;
        return repository_.query(query);
    }

    std::vector<Game> GameService::listGames() const
    {
        qCDebug(gamelogRuntimeLog) << "Returning all games";
        return search({});
    }

    std::vector<Game> GameService::listTrackedGames() const
    {
        qCDebug(gamelogRuntimeLog) << "Returning all tracked games";
        GameQuery query;
        query.trackingEnabled = true;
        return search(query);
    }

    std::optional<Game> GameService::findById(std::int64_t id) const
    {
        qCDebug(gamelogRuntimeLog) << "Returning game with id: " << id;
        GameQuery query;
        query.ids = {id};
        query.limit = 1;
        return firstOrNull(search(query));
    }

    std::optional<Game> GameService::findByExecutableName(const QString& name) const
    {
        qCDebug(gamelogRuntimeLog) << "Returning game with name: " << name;
        GameQuery query;
        query.executableName = name;
        query.limit = 1;
        return firstOrNull(search(query));
    }

    std::optional<Game> GameService::findByExecutablePath(const QString& path) const
    {
        qCDebug(gamelogRuntimeLog) << "Returning game with executable path: " << path;
        GameQuery query;
        query.executablePath = path;
        query.limit = 1;
        return firstOrNull(search(query));
    }

    bool GameService::addGame(Game& game)
    {
        qCDebug(gamelogRuntimeLog) << "Adding game: " << game;
        if(!repository_.insert(game)) { return false; }

        syncGamesWithDatabase();
        emit gameAdded(game);
        return true;
    }

    bool GameService::updateGame(const Game& game)
    {
        qCDebug(gamelogRuntimeLog) << "Updating game " << game;
        if(!repository_.update(game)) { return false; }

        syncGamesWithDatabase();
        return true;
    }

    bool GameService::removeGame(std::int64_t id)
    {
        qCDebug(gamelogRuntimeLog) << "Removing game with id: " << id;
        if(!repository_.remove(id)) { return false; }

        syncGamesWithDatabase();
        return true;
    }

    void GameService::syncGamesWithDatabase()
    {
        qCDebug(gamelogRuntimeLog) << "Syncing games with database";
        trackedPathGames_.clear();
        trackedSteamGames_.clear();

        for(const Game& game : listTrackedGames())
        {
            if(game.steamAppId && *game.steamAppId > 0)
            {
                trackedSteamGames_.insert(static_cast<std::uint32_t>(*game.steamAppId), game);
            }
            if(!game.executablePath.isEmpty()) { trackedPathGames_.insert(game.executablePath, game); }
        }

        qCInfo(gamelogRuntimeLog) << "Synced" << trackedSteamGames_.size() << "Steam games and" << trackedPathGames_.
            size() << "path-based games.";
    }

    const QHash<std::uint32_t, Game>& GameService::trackedSteamGames() const noexcept
    {
        qCDebug(gamelogRuntimeLog) << "Returning Steam tracked games";
        return trackedSteamGames_;
    }

    const QHash<QString, Game>& GameService::trackedPathGames() const noexcept
    {
        qCDebug(gamelogRuntimeLog) << "Returning path tracked games";
        return trackedPathGames_;
    }

    bool GameService::hasTrackedSteamGames() const noexcept
    {
        qCDebug(gamelogRuntimeLog) << "Checking if there are any tracked Steam games";
        return !trackedSteamGames_.isEmpty();
    }

    bool GameService::setHasArtwork(int gameId, bool hasArtwork)
    {
        auto game = findById(gameId);

        if(!game) { return false; }

        game->hasArtwork = hasArtwork;

        return updateGame(*game);
    }

    void GameService::syncSteamGames() { steamApiService_.getOwnedGames(); }

    void GameService::onSteamGamesReceived(const QJsonArray& steamGames)
    {
        qCInfo(gamelogRuntimeLog) << "Received" << steamGames.size() << "Steam games from API";

        for(const QJsonValue& value : steamGames)
        {
            if(!value.isObject()) { continue; }

            const QJsonObject object = value.toObject();

            const int appId = object.value(QStringLiteral("appid")).toInt();

            const QString title = object.value(QStringLiteral("name")).toString();

            if(appId <= 0 || title.isEmpty()) { continue; }

            Game game;

            game.title = title;
            game.steamAppId = appId;

            if(!trackedSteamGames_.contains(appId))
            {
                qCDebug(gamelogRuntimeLog) << "Adding game with steam id: " << appId;
                if(bool success = addGame(game); !success)
                {
                    qCWarning(gamelogRuntimeLog) << "Failed to add game with steam id: " << appId;
                }
            }
        }
    }
} // namespace gamelog::application::services
